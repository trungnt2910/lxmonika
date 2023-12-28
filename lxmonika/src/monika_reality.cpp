#include "monika.h"

#include <ntddk.h>
#include <ntstrsafe.h>

#include "lxerrno.h"
#include "lxss.h"
#include "module.h"
#include "os.h"

#include "Locker.h"
#include "Logger.h"
#include "PoolAllocator.h"

// lxcore.sys imports
//
// We cannot import them using __declspec(dllimport) since the WDK does not provide glue libs for
// lxcore.sys.
//
// We do not want to use lxcore.lib provided with the lxdk since that thing is LGPLv3 and
// statically linking to it would infect lxmonika.sys.
//
// We do not want to build our own lxcore.lib since it would require an entire different project
// (and we want to keep this repo "Just Monika" for as long as possible). Furthermore, a static
// import would not provide any significant benefit other than pushing symbol resolution to
// compile time - which is equally fragile for those private symbols, provided that our PE parsing
// implementation is correct.

typedef NTSTATUS LX_INITIALIZE(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PLX_SUBSYSTEM Subsystem
);
typedef LX_INITIALIZE* PLX_INITIALIZE;

typedef VOID LX_DEV_MISC_REGISTER(
    _In_ PLX_INSTANCE Instance,
    _In_ PLX_DEVICE Device,
    _In_ UINT32 DeviceMinor
);
typedef LX_DEV_MISC_REGISTER* PLX_DEV_MISC_REGISTER;

typedef INT LX_UTIL_TRANSLATE_STATUS(
    _In_ NTSTATUS Status
);
typedef LX_UTIL_TRANSLATE_STATUS* PLX_UTIL_TRANSLATE_STATUS;

typedef PLX_DEVICE VFS_DEVICE_MINOR_ALLOCATE(
    _In_ PLX_DEVICE_CALLBACKS Callbacks,
    _In_ SIZE_T Size
);
typedef VFS_DEVICE_MINOR_ALLOCATE* PVFS_DEVICE_MINOR_ALLOCATE;

typedef VOID VFS_DEVICE_MINOR_DEREFERENCE(
    _In_ PLX_DEVICE Device
);
typedef VFS_DEVICE_MINOR_DEREFERENCE* PVFS_DEVICE_MINOR_DEREFERENCE;

typedef PLX_FILE VFS_FILE_ALLOCATE(
    _In_ SIZE_T Size,
    _In_ PLX_FILE_CALLBACKS Callbacks
);
typedef VFS_FILE_ALLOCATE* PVFS_FILE_ALLOCATE;

static PLX_INITIALIZE                   LxInitialize                = NULL;
static PLX_DEV_MISC_REGISTER            LxpDevMiscRegister          = NULL;
static PLX_UTIL_TRANSLATE_STATUS        LxpUtilTranslateStatus      = NULL;
static PVFS_DEVICE_MINOR_ALLOCATE       VfsDeviceMinorAllocate      = NULL;
static PVFS_DEVICE_MINOR_DEREFERENCE    VfsDeviceMinorDereference   = NULL;
static PVFS_FILE_ALLOCATE               VfsFileAllocate             = NULL;

//
// Private function prototypes
//

static LX_SUBSYSTEM_CREATE_INITIAL_NAMESPACE    MaRealityCreateInitialNamespace;
static LX_DEVICE_OPEN                           MaDeviceOpen;
static LX_DEVICE_DELETE                         MaDeviceDelete;
static LX_FILE_DELETE                           MaFileDelete;
static LX_FILE_READ                             MaFileRead;
static LX_FILE_WRITE                            MaFileWrite;
static LX_FILE_IOCTL                            MaFileIoctl;
static LX_FILE_FLUSH                            MaFileFlush;
static LX_FILE_SEEK                             MaFileSeek;

//
// Lifetime functions
//

#define MA_TRY_RESOLVE_SYMBOL(name)                                                             \
    do                                                                                          \
    {                                                                                           \
        status = MdlpGetProcAddress(hdlLxCore, #name, (PVOID*)&name);                           \
        if (!NT_SUCCESS(status))                                                                \
        {                                                                                       \
            Logger::LogError("Failed to resolve " #name);                                       \
        }                                                                                       \
    } while (false)

extern "C"
NTSTATUS
MapInitializeLxssDevice(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    NTSTATUS status;

    HANDLE hdlLxCore;
    SIZE_T uLxCoreSize;
    status = MdlpFindModuleByName("lxcore.sys", &hdlLxCore, &uLxCoreSize);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    MA_TRY_RESOLVE_SYMBOL(LxInitialize);
    MA_TRY_RESOLVE_SYMBOL(LxpDevMiscRegister);
    MA_TRY_RESOLVE_SYMBOL(LxpUtilTranslateStatus);
    MA_TRY_RESOLVE_SYMBOL(VfsDeviceMinorAllocate);
    MA_TRY_RESOLVE_SYMBOL(VfsDeviceMinorDereference);
    MA_TRY_RESOLVE_SYMBOL(VfsFileAllocate);

    LX_SUBSYSTEM subsystem =
    {
        .CreateInitialNamespace = MaRealityCreateInitialNamespace
    };

    // TODO: Potential clash with lxdk-based drivers at this point?
    status = LxInitialize(DriverObject, &subsystem);

    if (!NT_SUCCESS(status) && status != STATUS_TOO_LATE)
    {
        return status;
    }

    return STATUS_SUCCESS;
}

extern "C"
VOID
MapCleanupLxssDevice()
{
    // Currently no-op.
}

//
// VFS structs
//

typedef struct _MA_FILE : public LX_FILE {
    FAST_MUTEX  Lock;
    SIZE_T      Length;
    SIZE_T      Offset;
    CHAR        Data[2048];
} MA_FILE, *PMA_FILE;

//
// VFS data
//

static LX_DEVICE_CALLBACKS MaVfsDeviceCallbacks =
{
    .Open = MaDeviceOpen,
    .Delete = MaDeviceDelete
};

static LX_FILE_CALLBACKS MaVfsFileCallbacks =
{
    .Delete = MaFileDelete,
    .Read = MaFileRead,
    .Write = MaFileWrite,
    .Ioctl = MaFileIoctl,
    .Flush = MaFileFlush,
    .Seek = MaFileSeek
};

//
// Reality information
//

static
NTSTATUS
MaUpdateFileInformation(
    _Inout_ PMA_FILE pFile
)
{
    PMA_CONTEXT pContext = (PMA_CONTEXT)
        MapOriginalRoutines.GetThreadContext(PsGetCurrentThread());

    ExAcquireFastMutex(&pFile->Lock);

    // Does not count the null terminator.
    SIZE_T uSizeLeft = sizeof(pFile->Data) - 1;
    pFile->Length = 0;

    const auto Write = [&](SIZE_T uTheoreticalSize)
    {
        SIZE_T uWrittenSize = min(uTheoreticalSize, uSizeLeft);
        uSizeLeft -= uWrittenSize;
        pFile->Length += uWrittenSize;

        if (uSizeLeft > 0)
        {
            --uSizeLeft;
            pFile->Data[pFile->Length] = '\n';
            ++pFile->Length;
        }
    };

    // Check if the context is valid.
    if (pContext != NULL && pContext->Magic == MA_CONTEXT_MAGIC)
    {
        // Since MapProviderNames is not guarded, this can cause a potential data race when a
        // provider changes its name while a process is running and holding a reference to a
        // reality file.
        // However, we have explicitly declared this to be undefined behavior in
        // MaSetPicoProviderName.
        // The worst thing this can cause is gibberish being printed out for this field. A buffer
        // overrun cannot occur since the last byte (index MA_NAME_MAX) of every entry is
        // guaranteed to be null and untouched.
        Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
            "ProviderName:\t%s", MapProviderNames[pContext->Provider]
        ));
        Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
            "ProviderId:\t%d", pContext->Provider
        ));

        // Check if the context has a parent
        if (pContext->Parent != NULL)
        {
            Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
                "ProviderHasParent:\t1"
            ));
            // And the parent is also valid.
            if (pContext->Parent->Magic == MA_CONTEXT_MAGIC)
            {
                Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
                    "ProviderParentName:\t%s", MapProviderNames[pContext->Parent->Provider]
                ));
                Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
                    "ProviderParentId:\t%d", pContext->Parent->Provider
                ));
            }
        }
    }

    Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
        "MaProvidersCnt:\t%zu", MapProvidersCount
    ));

    Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
        "MaProvidersMax:\t%zu", (SIZE_T)MaPicoProviderMaxCount
    ));

#ifdef MONIKA_TIMESTAMP
    Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
        "MaBuildTime:\t" MONIKA_TIMESTAMP
    ));
#endif

#ifdef MONIKA_BUILD_NUMBER
    Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
        "MaBuildNumber:\t" MONIKA_BUILD_NUMBER
    ));
#endif

#ifdef MONIKA_BUILD_HASH
    Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
        "MaBuildHash:\t" MONIKA_BUILD_HASH
    ));
#endif

#ifdef MONIKA_BUILD_TAG
    Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
        "MaBuildTag:\t" MONIKA_BUILD_TAG
    ));
#endif

#ifdef MONIKA_BUILD_ORIGIN
    Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
        "MaBuildOrigin:\t" MONIKA_BUILD_ORIGIN
    ));
#endif

#ifndef MONIKA_BUILD_AUTHOR
#define MONIKA_BUILD_AUTHOR "lxmonika Authors & Contributors"
#endif

#ifndef MONIKA_BUILD_YEAR
#define MONIKA_BUILD_YEAR   (&__DATE__[7])
#endif

    if (strcmp(MONIKA_BUILD_YEAR, "2023") == 0)
    {
        Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
            "MaCopyright:\tCopyright (C) 2023 %s", MONIKA_BUILD_AUTHOR
        ));
    }
    else
    {
        Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
            "MaCopyright:\tCopyright (C) 2023-%s %s", MONIKA_BUILD_YEAR, MONIKA_BUILD_AUTHOR
        ));
    }

    pFile->Data[pFile->Length] = '\0';
    ++pFile->Length;

    ExReleaseFastMutex(&pFile->Lock);

    return STATUS_SUCCESS;
}

//
// Driver VFS functions
//

NTSTATUS
MapRealityEscape();

static
INT
MaRealityCreateInitialNamespace(
    _In_ PLX_INSTANCE pInstance
)
{
    Logger::LogTrace("CreateInitialNamespace callback called.");

    PLX_DEVICE pDevice = VfsDeviceMinorAllocate(&MaVfsDeviceCallbacks, sizeof(*pDevice));

    if (pDevice == NULL)
    {
        Logger::LogError("Failed to allocate reality device.");
        return -LINUX_ENOMEM;
    }

    LxpDevMiscRegister(pInstance, pDevice, MA_REALITY_MINOR);

    Logger::LogTrace("CreateInitialNamespace successful.");

    // TODO: Register the /dev/reality device.
    // Theoretically, this could be done by `VfsInitializeStartupEntries`.
    // However:
    // - Previous attempts to use this function to create a single entry, `/dev/reality`
    // at this exact point would fail with the status code -17 (LINUX_EEXIST).
    // - According to https://github.com/billziss-gh/winfuse/releases, the WSL1 `init`
    // process would remount the `/dev` filesystem, making the entry overwritten even after
    // success.
    // It is possible for us to intercept the birth of the first Pico process and then force the
    // creation of the device by faking a `mknod` syscall. However, this is probably unnecessary,
    // since switching from WSL1 to another provider using the `ioctl` method would require a
    // helper process spwaning right after `init` anyway.

    return 0;
}

static
INT
MaDeviceOpen(
    _In_ PLX_CALL_CONTEXT pContext,
    _In_ PLX_DEVICE pDevice,
    _In_ ULONG uFlags,
    _Out_ PLX_FILE* pFile
)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pDevice);
    UNREFERENCED_PARAMETER(uFlags);
    UNREFERENCED_PARAMETER(uFlags);

    PMA_FILE pNewFile = (PMA_FILE)VfsFileAllocate(sizeof(*pNewFile), &MaVfsFileCallbacks);

    if (pNewFile == NULL)
    {
        return -LINUX_ENOMEM;
    }

    ExInitializeFastMutex(&pNewFile->Lock);

    pNewFile->Length = 0;
    pNewFile->Offset = 0;
    pNewFile->Data[0] = '\0';

    NTSTATUS status = MaUpdateFileInformation(pNewFile);

    if (!NT_SUCCESS(status))
    {
        // Since VfsFileDeallocate or the like is not exposed, we have no way to clean up
        // the allocated file and cleanly fail here.
        Logger::LogWarning("Failed to update file information, status=", (PVOID)status);
    }

    *pFile = pNewFile;

    return 0;
}

static
INT
MaDeviceDelete(
    _Inout_ PLX_DEVICE pDevice
)
{
    UNREFERENCED_PARAMETER(pDevice);

    return 0;
}

static
INT
MaFileDelete(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile
)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pFile);

    return 0;
}

static
INT
MaFileRead(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile,
    _Out_ PVOID pBuffer,
    _In_ SIZE_T uLength,
    _Inout_opt_ POFF_T pOffset,
    _Out_ PSIZE_T pBytesTransferred
)
{
    UNREFERENCED_PARAMETER(pContext);

    PMA_FILE pMaFile = (PMA_FILE)pFile;
    *pBytesTransferred = 0;

    NTSTATUS status = MaUpdateFileInformation(pMaFile);

    if (!NT_SUCCESS(status))
    {
        return LxpUtilTranslateStatus(status);
    }

    ExAcquireFastMutex(&pMaFile->Lock);

    __try
    {
        __try
        {
            OFF_T oStart;
            if (pOffset == NULL)
            {
                oStart = pMaFile->Offset;
            }
            else
            {
                oStart = *pOffset;
            }

            // Some kind of faulty pread.
            // pMaFile->Offset is guaranteed to be non-negative.
            if (oStart < 0)
            {
                return -LINUX_EINVAL;
            }

            if ((SIZE_T)oStart >= pMaFile->Length)
            {
                *pBytesTransferred = 0;
                return 0;
            }

            OFF_T oEnd = min(oStart + uLength, pMaFile->Length);

            memcpy(pBuffer, pMaFile->Data + oStart, oEnd - oStart);

            if (pOffset == NULL)
            {
                pMaFile->Offset = oEnd;
            }
            else
            {
                *pOffset = oEnd;
            }

            *pBytesTransferred = oEnd - oStart;

            return 0;
        }
        __finally
        {
            ExReleaseFastMutex(&pMaFile->Lock);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -LINUX_EFAULT;
    }
}

static
INT
MaFileWrite(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile,
    _In_ PVOID pBuffer,
    _In_ SIZE_T uLength,
    _Inout_ POFF_T pOffset,
    _Out_ PSIZE_T pBytesTransferred
)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pFile);
    UNREFERENCED_PARAMETER(pOffset);

    *pBytesTransferred = 0;

    __try
    {
        PCHAR pCharBuffer = (PCHAR)pBuffer;
        SIZE_T uPatternLength = sizeof("love") - 1;
        for (SIZE_T i = 0; i + uPatternLength < uLength; ++i)
        {
            bool equal = true;
            for (SIZE_T j = 0; j < uPatternLength; ++j)
            {
                equal = equal && (tolower(pCharBuffer[i + j]) == "love"[j]);
            }
            if (equal)
            {
#ifdef DBG
                MapRealityEscape();
#endif
                return -LINUX_EPERM;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -LINUX_EFAULT;
    }

    *pBytesTransferred = uLength;
    return 0;
}

static
INT
MaFileIoctl(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile,
    _In_ ULONG uCode,
    _Inout_ PVOID pBuffer
)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pFile);

    __try
    {
        switch (uCode)
        {
            case MA_IOCTL_SET_PROVIDER:
            {
                PCSTR pRequestedProvider = (PCSTR)pBuffer;
                Logger::LogTrace("MA_IOCTL_SET_PROVIDER ", pRequestedProvider);

                SIZE_T uProviderLength = strnlen(pRequestedProvider, MA_NAME_MAX);

                // Prevent reading newer providers.
                SIZE_T uCurrentRegisteredCount = min(MapProvidersCount, MaPicoProviderMaxCount);

                SIZE_T uBestMatchLength = 0;
                DWORD uBestMatchIndex = 0;

                // Potential race condition in this loop while reading the names,
                // discussed in more details in other places.
                for (SIZE_T i = 0; i < uCurrentRegisteredCount; ++i)
                {
                    SIZE_T uCurrentMatch = RtlCompareMemory(pBuffer, MapProviderNames[i],
                        min(uProviderLength, sizeof(MapProviderNames[i])));

                    if (uCurrentMatch > uBestMatchLength)
                    {
                        uBestMatchLength = uCurrentMatch;
                        uBestMatchIndex = (DWORD)i;
                    }
                }

                if (uBestMatchLength == 0)
                {
                    // Nothing actually matches!
                    return -LINUX_EINVAL;
                }

                PMA_CONTEXT pMaContext = (PMA_CONTEXT)
                    MapOriginalRoutines.GetThreadContext(PsGetCurrentThread());

                if (pMaContext != NULL && pMaContext->Magic == MA_CONTEXT_MAGIC)
                {
                    if (pMaContext->Provider != uBestMatchIndex)
                    {
                        PMA_CONTEXT pNewContext = MapAllocateContext(uBestMatchIndex, NULL);
                        if (pNewContext == NULL)
                        {
                            return -LINUX_ENOMEM;
                        }

                        NTSTATUS status = MapPushContext(pMaContext, pNewContext);

                        if (!NT_SUCCESS(status))
                        {
                            return LxpUtilTranslateStatus(status);
                        }
                    }
                }
                else
                {
                    // Can't switch a process unless it's managed by Monika.
                    Logger::LogError("Attempted to switch providers for "
                        "a process not managed by lxmonika");
                    return -LINUX_EPERM;
                }

                Logger::LogTrace("Successfully switched process to provider ID ", uBestMatchIndex);
                Logger::LogTrace("New provider name: ", MapProviderNames[uBestMatchIndex]);

                return 0;
            }
            break;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -LINUX_EFAULT;
    }

    return -LINUX_EINVAL;
}

static
INT
MaFileFlush(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile
)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pFile);

    return 0;
}

static
INT
MaFileSeek(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile,
    _In_ OFF_T offset,
    _In_ INT whence,
    _Out_ POFF_T pResultOffset
)
{
    UNREFERENCED_PARAMETER(pContext);

    PMA_FILE pMaFile = (PMA_FILE)pFile;
    INT Error = 0;

    ExAcquireFastMutex(&pMaFile->Lock);

    OFF_T newOffset = pMaFile->Offset;

    switch (whence)
    {
        case SEEK_SET:
        {
            newOffset = offset;
        }
        break;
        case SEEK_CUR:
        {
            newOffset += offset;
        }
        break;
        case SEEK_END:
        {
            newOffset = pMaFile->Length + offset;
        }
        break;
        default:
        {
            Error = -LINUX_EINVAL;
        }
    }

    if (newOffset < 0)
    {
        Error = -LINUX_EINVAL;
    }
    else
    {
        pMaFile->Offset = newOffset;
        *pResultOffset = newOffset;
    }

    ExReleaseFastMutex(&pMaFile->Lock);

    return Error;
}

NTSTATUS
MapRealityEscape()
{
    // In Windows 10, the .rsrc section is no longer stored after the early boot stages.
    // Therefore, `MdlpGetMessageTable` would not work.

    NTSTATUS status;
    PMESSAGE_RESOURCE_DATA pMessageTable = NULL;

    // Initial buffer size. Pure guesswork.
    // The usual technique of passing a dummy buffer to query the length of the real information
    // somehow does not work for SystemBigPoolInformation.
    ULONG uLen = 1 << 18;

    while (TRUE)
    {
        PoolAllocator pBigpoolAllocator(uLen, 'eRaM');
        if (pBigpoolAllocator.Get() == NULL)
        {
            return STATUS_NO_MEMORY;
        }

        PSYSTEM_BIGPOOL_INFORMATION pInfo = pBigpoolAllocator.Get<SYSTEM_BIGPOOL_INFORMATION>();

        Logger::LogTrace("Attempting to get the system pool information using a buffer of ",
            uLen, " bytes.");

        status = ZwQuerySystemInformation(SystemBigPoolInformation, pInfo, uLen, NULL);
        if (!NT_SUCCESS(status))
        {
            if (status == STATUS_INFO_LENGTH_MISMATCH)
            {
                Logger::LogTrace("Query failed. Doubling the size...");
                uLen <<= 1;
                continue;
            }
            return status;
        }

        for (SIZE_T i = 0; i < pInfo->Count; ++i)
        {
            if (pInfo->AllocatedInfo[i].TagUlong == 'cBiK')
            {
                if (pInfo->AllocatedInfo[i].SizeInBytes > MAXULONG)
                {
                    continue;
                }

                // The match is ambiguous!
                if (pMessageTable != NULL)
                {
                    Logger::LogError("Found multiple matches when searching for message table.");
                    Logger::LogError("First match: ", (PVOID)pMessageTable);
                    Logger::LogError("Current match: ", pInfo->AllocatedInfo[i].VirtualAddress);
                    return STATUS_BAD_DATA;
                }

                pMessageTable = (PMESSAGE_RESOURCE_DATA)
                    ALIGN_DOWN_POINTER_BY(pInfo->AllocatedInfo[i].VirtualAddress, 2);
            }
        }

        if (pMessageTable == NULL)
        {
            return STATUS_NOT_FOUND;
        }

        break;
    }

    CONST CHAR pMagicBytes[] =
    {
        76, 88, 83, 83, 95,
        69, 83, 67, 65, 80, 69, 95,
        80, 76, 65, 78, 95,
        70, 65, 73, 76, 69, 68,
        13, 10, 00
    };
    CONST SIZE_T uMinLength = sizeof(pMagicBytes) + FIELD_OFFSET(MESSAGE_RESOURCE_ENTRY, Text);

    for (SIZE_T i = 0; i < pMessageTable->NumberOfBlocks; ++i)
    {
        PMESSAGE_RESOURCE_BLOCK pMessageBlock = &pMessageTable->Blocks[i];
        PMESSAGE_RESOURCE_ENTRY pMessageEntry = (PMESSAGE_RESOURCE_ENTRY)
            ((PCHAR)pMessageTable + pMessageBlock->OffsetToEntries);

        const auto GetNextEntry = [](PMESSAGE_RESOURCE_ENTRY pEntry)
        {
            return (PMESSAGE_RESOURCE_ENTRY)((PCHAR)pEntry + pEntry->Length);
        };

        for (SIZE_T j = 0;
            j < (pMessageBlock->HighId - pMessageBlock->LowId + 1);
            ++j, pMessageEntry = GetNextEntry(pMessageEntry))
        {
            if (pMessageEntry->Flags & MESSAGE_RESOURCE_UNICODE)
            {
                continue;
            }

            if (pMessageEntry->Length < uMinLength)
            {
                continue;
            }

            BOOLEAN bShouldPatch = TRUE;

            for (SIZE_T k = 0; pMessageEntry->Text[k] != '\0'; ++k)
            {
                CHAR ch = pMessageEntry->Text[k];
                if (!isupper(ch)
                    && !isdigit(ch)
                    // Accept trailing line terminators, but not whitespaces.
                    && !(iswspace(ch) && ch != ' ')
                    && ch != '_')
                {
                    Logger::LogTrace("Violating character: ", (DWORD)ch);
                    bShouldPatch = FALSE;
                    break;
                }
            }

            if (bShouldPatch)
            {
                strcpy((PCHAR)pMessageEntry->Text, pMagicBytes);
                memset(((PCHAR)pMessageEntry) + uMinLength, 0, pMessageEntry->Length - uMinLength);
            }
        }
    }

    // Magic code to escape into your reality
    (VOID)KeRaiseIrqlToDpcLevel();
    *(volatile UCHAR*)NULL = 0;
    RtlFailFast(MA_REALITY_MINOR);
}
