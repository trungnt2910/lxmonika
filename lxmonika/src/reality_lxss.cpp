#include "reality.h"

#include "lxerrno.h"
#include "lxss.h"
#include "module.h"
#include "monika.h"

#include "Logger.h"

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

static PLX_INITIALIZE                   LxInitialize = NULL;
static PLX_DEV_MISC_REGISTER            LxpDevMiscRegister = NULL;
static PLX_UTIL_TRANSLATE_STATUS        LxpUtilTranslateStatus = NULL;
static PVFS_DEVICE_MINOR_ALLOCATE       VfsDeviceMinorAllocate = NULL;
static PVFS_DEVICE_MINOR_DEREFERENCE    VfsDeviceMinorDereference = NULL;
static PVFS_FILE_ALLOCATE               VfsFileAllocate = NULL;

//
// Private function prototypes
//

static LX_SUBSYSTEM_CREATE_INITIAL_NAMESPACE    RlCreateInitialLxssNamespace;
static LX_DEVICE_OPEN                           RlLxssDeviceOpen;
static LX_DEVICE_DELETE                         RlLxssDeviceDelete;
static LX_FILE_DELETE                           RlLxssFileDelete;
static LX_FILE_READ                             RlLxssFileRead;
static LX_FILE_WRITE                            RlLxssFileWrite;
static LX_FILE_IOCTL                            RlLxssFileIoctl;
static LX_FILE_FLUSH                            RlLxssFileFlush;
static LX_FILE_SEEK                             RlLxssFileSeek;

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
RlpInitializeLxssDevice(
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
        .CreateInitialNamespace = RlCreateInitialLxssNamespace
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
RlpCleanupLxssDevice()
{
    // Currently no-op.
}

//
// VFS structs
//

typedef struct _MA_FILE : public LX_FILE {
    RL_FILE File;
} MA_FILE, * PMA_FILE;

//
// VFS data
//

static LX_DEVICE_CALLBACKS RlLxssVfsDeviceCallbacks =
{
    .Open = RlLxssDeviceOpen,
    .Delete = RlLxssDeviceDelete
};

static LX_FILE_CALLBACKS RlLxssVfsFileCallbacks =
{
    .Delete = RlLxssFileDelete,
    .Read = RlLxssFileRead,
    .Write = RlLxssFileWrite,
    .Ioctl = RlLxssFileIoctl,
    .Flush = RlLxssFileFlush,
    .Seek = RlLxssFileSeek
};

//
// Driver VFS functions
//
// These functions must return LXSS error codes!
//

static
INT
RlCreateInitialLxssNamespace(
    _In_ PLX_INSTANCE pInstance
)
{
    Logger::LogTrace("CreateInitialNamespace callback called.");

    PLX_DEVICE pDevice = VfsDeviceMinorAllocate(&RlLxssVfsDeviceCallbacks, sizeof(*pDevice));

    if (pDevice == NULL)
    {
        Logger::LogError("Failed to allocate reality device.");
        return -LINUX_ENOMEM;
    }

    LxpDevMiscRegister(pInstance, pDevice, RL_DEVICE_CODE);

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
LXSTATUS
RlLxssDeviceOpen(
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

    PMA_FILE pNewFile = (PMA_FILE)VfsFileAllocate(sizeof(*pNewFile), &RlLxssVfsFileCallbacks);

    if (pNewFile == NULL)
    {
        return -LINUX_ENOMEM;
    }

    NTSTATUS status = RlpFileOpen(&pNewFile->File);

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
LXSTATUS
RlLxssDeviceDelete(
    _Inout_ PLX_DEVICE pDevice
)
{
    UNREFERENCED_PARAMETER(pDevice);

    return 0;
}

static
LXSTATUS
RlLxssFileDelete(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile
)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pFile);

    return 0;
}

static
LXSTATUS
RlLxssFileRead(
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

    NTSTATUS status = RlpFileRead(&pMaFile->File, pBuffer, uLength, pOffset, pBytesTransferred);

    if (!NT_SUCCESS(status))
    {
        return LxpUtilTranslateStatus(status);
    }

    return 0;
}

static
LXSTATUS
RlLxssFileWrite(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile,
    _In_ PVOID pBuffer,
    _In_ SIZE_T uLength,
    _Inout_ POFF_T pOffset,
    _Out_ PSIZE_T pBytesTransferred
)
{
    UNREFERENCED_PARAMETER(pContext);

    PMA_FILE pMaFile = (PMA_FILE)pFile;

    NTSTATUS status = RlpFileWrite(&pMaFile->File, pBuffer, uLength, pOffset, pBytesTransferred);

    if (!NT_SUCCESS(status))
    {
        return LxpUtilTranslateStatus(status);
    }

    return 0;
}

static
LXSTATUS
RlLxssFileIoctl(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile,
    _In_ ULONG uCode,
    _Inout_ PVOID pBuffer
)
{
    UNREFERENCED_PARAMETER(pContext);

    switch (uCode)
    {
    // Deprecated ioctl that allows WSL processes to magically switch to other providers.
    case MA_IOCTL_SET_PROVIDER:
    {
        __try
        {
            PCSTR pRequestedProvider = (PCSTR)pBuffer;
            Logger::LogTrace("MA_IOCTL_SET_PROVIDER ", pRequestedProvider);

            UTF8_STRING strUtf8RequestedProvider;
            RtlInitUTF8String(&strUtf8RequestedProvider, (PCSZ)pBuffer);

            UNICODE_STRING strUnicodeRequestedProvider;
            NTSTATUS status = RtlUTF8StringToUnicodeString(
                &strUnicodeRequestedProvider, &strUtf8RequestedProvider, TRUE);

            if (!NT_SUCCESS(status))
            {
                return LxpUtilTranslateStatus(status);
            }

            SIZE_T uNewIndex;
            status = MaFindPicoProvider(strUnicodeRequestedProvider.Buffer, &uNewIndex);
            RtlFreeUnicodeString(&strUnicodeRequestedProvider);

            if (!NT_SUCCESS(status))
            {
                return LxpUtilTranslateStatus(status);
            }

            PMA_CONTEXT pMaContext = (PMA_CONTEXT)
                MapOriginalRoutines.GetThreadContext(PsGetCurrentThread());

            if (pMaContext != NULL && pMaContext->Magic == MA_CONTEXT_MAGIC)
            {
                if (pMaContext->Provider != uNewIndex)
                {
                    PMA_CONTEXT pNewContext = MapAllocateContext((DWORD)uNewIndex, NULL);
                    if (pNewContext == NULL)
                    {
                        return -LINUX_ENOMEM;
                    }

                    status = MapPushContext(pMaContext, pNewContext);

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

            Logger::LogTrace("Successfully switched process to provider ID ", uNewIndex);

            return 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return -LINUX_EFAULT;
        }
    }
    break;
    // Other ioctls follow the codes defined in reality.h and is shared with the Win32 driver.
    default:
    {
        PMA_FILE pMaFile = (PMA_FILE)pFile;

        NTSTATUS status = RlpFileIoctl(&pMaFile->File, uCode, pBuffer);

        if (!NT_SUCCESS(status))
        {
            return LxpUtilTranslateStatus(status);
        }

        return 0;
    }
    }
}

static
LXSTATUS
RlLxssFileFlush(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile
)
{
    UNREFERENCED_PARAMETER(pContext);
    UNREFERENCED_PARAMETER(pFile);

    return 0;
}

static
LXSTATUS
RlLxssFileSeek(
    _In_ PLX_CALL_CONTEXT pContext,
    _Inout_ PLX_FILE pFile,
    _In_ OFF_T offset,
    _In_ INT whence,
    _Out_ POFF_T pResultOffset
)
{
    UNREFERENCED_PARAMETER(pContext);

    PMA_FILE pMaFile = (PMA_FILE)pFile;

    NTSTATUS status = RlpFileSeek(&pMaFile->File, offset, whence, pResultOffset);

    if (!NT_SUCCESS(status))
    {
        return LxpUtilTranslateStatus(status);
    }

    return 0;
}
