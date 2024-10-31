#include "reality.h"

#include <ntifs.h>
#include <ntstrsafe.h>

#include "condrv.h"
#include "lxerrno.h"
#include "lxss.h"
#include "module.h"
#include "monika.h"
#include "os.h"

#include "AutoResource.h"
#include "Locker.h"
#include "Logger.h"
#include "PoolAllocator.h"

//
// Utility forward declarations
//

static
NTSTATUS
    RlFileUpdateInformation(
        _Inout_ PRL_FILE pFile
    );

NTSTATUS
    RlEscape();

//
// Lifetime functions
//

extern "C"
NTSTATUS
RlpInitializeDevices(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    NTSTATUS status;

    status = RlpInitializeWin32Device(DriverObject);

    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = RlpInitializeLxssDevice(DriverObject);

    if (!NT_SUCCESS(status))
    {
        Logger::LogWarning("Failed to initialize lxss device, status=", (PVOID)status);
        Logger::LogWarning("WSL integration features will not work.");
        Logger::LogWarning("Make sure WSL1 is enabled and lxcore.sys is loaded.");

        // Non-fatal.
    }

    return STATUS_SUCCESS;
}

extern "C"
VOID
RlpCleanupDevices()
{
    RlpCleanupLxssDevice();
    RlpCleanupWin32Device();
}

//
// Reality files
//

extern "C"
NTSTATUS
RlpFileOpen(
    _Out_ PRL_FILE pFile
)
{
    if (pFile == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ExInitializeFastMutex(&pFile->Lock);

    pFile->Length = 0;
    pFile->Offset = 0;
    pFile->Data[0] = '\0';

    MA_RETURN_IF_FAIL(RlFileUpdateInformation(pFile));

    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
RlpFileRead(
    _Inout_ PRL_FILE pFile,
    _Out_writes_bytes_(szLength) PVOID pBuffer,
    _In_ SIZE_T szLength,
    _Inout_opt_ PINT64 pOffset,
    _Out_opt_ PSIZE_T pBytesTransferred
)
{
    if (pFile == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (pBytesTransferred != NULL)
    {
        *pBytesTransferred = 0;
    }

    MA_RETURN_IF_FAIL(RlFileUpdateInformation(pFile));

    ExAcquireFastMutex(&pFile->Lock);

    __try
    {
        __try
        {
            INT64 oStart;
            if (pOffset == NULL)
            {
                oStart = pFile->Offset;
            }
            else
            {
                oStart = *pOffset;
            }

            // Some kind of faulty pread.
            // pFile->Offset is guaranteed to be non-negative.
            if (oStart < 0)
            {
                return STATUS_INVALID_PARAMETER;
            }

            if (oStart >= (INT64)pFile->Length)
            {
                if (pBytesTransferred != NULL)
                {
                    *pBytesTransferred = 0;
                }
                return 0;
            }

            INT64 oEnd = min(oStart + szLength, pFile->Length);

            memcpy(pBuffer, pFile->Data + oStart, oEnd - oStart);

            if (pOffset == NULL)
            {
                pFile->Offset = oEnd;
            }
            else
            {
                *pOffset = oEnd;
            }

            if (pBytesTransferred != NULL)
            {
                *pBytesTransferred = oEnd - oStart;
            }

            return 0;
        }
        __finally
        {
            ExReleaseFastMutex(&pFile->Lock);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return STATUS_ACCESS_VIOLATION;
    }
}

extern "C"
NTSTATUS
RlpFileWrite(
    _Inout_ PRL_FILE pFile,
    _In_reads_bytes_(szLength) PVOID pBuffer,
    _In_ SIZE_T szLength,
    _Inout_ PINT64 pOffset,
    _Out_opt_ PSIZE_T pBytesTransferred
)
{
    UNREFERENCED_PARAMETER(pFile);
    UNREFERENCED_PARAMETER(pOffset);

    if (pBytesTransferred != NULL)
    {
        *pBytesTransferred = 0;
    }

    __try
    {
        PCHAR pCharBuffer = (PCHAR)pBuffer;
        SIZE_T uPatternLength = sizeof("love") - 1;
        for (SIZE_T i = 0; i + uPatternLength <= szLength; ++i)
        {
            bool equal = true;
            for (SIZE_T j = 0; j < uPatternLength; ++j)
            {
                equal = equal && (tolower(pCharBuffer[i + j]) == "love"[j]);
            }
            if (equal)
            {
#ifdef DBG
                RlEscape();
#endif
                // Corresponds to EPERM according to LxpUtilTranslateStatus.
                return STATUS_NOT_SUPPORTED;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return STATUS_ACCESS_VIOLATION;
    }

    if (pBytesTransferred != NULL)
    {
        *pBytesTransferred = szLength;
    }

    return STATUS_SUCCESS;
}

extern "C"
NTSTATUS
RlpFileIoctl(
    _Inout_ PRL_FILE pFile,
    _In_ ULONG ulCode,
    _Inout_ PVOID pData
)
{
    UNREFERENCED_PARAMETER(pFile);

    switch (ulCode)
    {
    case RlIoctlPicoStartSession:
    {
        PRL_PICO_SESSION_ATTRIBUTES pUserAttributes = (PRL_PICO_SESSION_ATTRIBUTES)pData;

        OBJECT_ATTRIBUTES objectAttributes;
        IO_STATUS_BLOCK ioStatus;

        // Declare handles and their cleaners.

        HANDLE hdlHostProcess = NULL;
        HANDLE hdlConsole = NULL;
        HANDLE hdlInput = NULL;
        HANDLE hdlOutput = NULL;
        HANDLE hdlRootDirectory = NULL;
        HANDLE hdlCurrentWorkingDirectory = NULL;

        AUTO_RESOURCE(hdlHostProcess, ZwClose);
        AUTO_RESOURCE(hdlConsole, ZwClose);
        AUTO_RESOURCE(hdlInput, ZwClose);
        AUTO_RESOURCE(hdlOutput, ZwClose);
        AUTO_RESOURCE(hdlRootDirectory, ZwClose);
        AUTO_RESOURCE(hdlCurrentWorkingDirectory, ZwClose);

        // Declare strings and their cleaners.

        UNICODE_STRING strRootDirectory = { 0 };
        UNICODE_STRING strCurrentWorkingDirectory = { 0 };

        // Make some pointers, since AUTO_RESOURCE only works with pointers.
        PUNICODE_STRING pStrRootDirectory = &strRootDirectory;
        PUNICODE_STRING pStrCurrentWorkingDirectory = &strCurrentWorkingDirectory;

        static constexpr auto FreeString = [](auto pStr)
        {
            if (pStr->Buffer != NULL)
            {
                ExFreePoolWithTag(pStr->Buffer, MA_REALITY_TAG);
            }
        };
        AUTO_RESOURCE(pStrRootDirectory, FreeString);
        AUTO_RESOURCE(pStrCurrentWorkingDirectory, FreeString);

        struct _MA_UNICODE_STRING_LIST
        {
            PUNICODE_STRING Strings = NULL;
            SIZE_T Length = 0;

            ~_MA_UNICODE_STRING_LIST()
            {
                if (Strings != NULL)
                {
                    for (SIZE_T i = Length - 1; i != (SIZE_T)-1; --i)
                    {
                        FreeString(&Strings[i]);
                    }
                    ExFreePoolWithTag(Strings, MA_REALITY_TAG);
                }
            }
        } strListProviderArgs, strListArgs, strListEnvironment;

        SIZE_T uProviderIndex;

        __try
        {
            if (pUserAttributes->Size != sizeof(RL_PICO_SESSION_ATTRIBUTES))
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }

            if (pUserAttributes->ProviderIndex < MaPicoProviderMaxCount)
            {
                uProviderIndex = pUserAttributes->ProviderIndex;
            }
            else
            {
                MA_RETURN_IF_FAIL(MaFindPicoProvider(
                    pUserAttributes->ProviderName->Buffer,
                    &uProviderIndex)
                );
            }

            // Convert these paths into handles

            InitializeObjectAttributes(
                &objectAttributes,
                pUserAttributes->RootDirectory,
                OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                NULL,
                NULL
            );

            MA_RETURN_IF_FAIL(ZwCreateFile(
                &hdlRootDirectory,
                FILE_LIST_DIRECTORY | FILE_TRAVERSE,
                &objectAttributes,
                &ioStatus,
                NULL,
                0,
                FILE_SHARE_VALID_FLAGS,
                FILE_OPEN,
                FILE_DIRECTORY_FILE,
                NULL,
                0
            ));

            InitializeObjectAttributes(
                &objectAttributes,
                pUserAttributes->CurrentWorkingDirectory,
                OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                NULL,
                NULL
            );

            MA_RETURN_IF_FAIL(ZwCreateFile(
                &hdlCurrentWorkingDirectory,
                FILE_LIST_DIRECTORY | FILE_TRAVERSE,
                &objectAttributes,
                &ioStatus,
                NULL,
                0,
                FILE_SHARE_VALID_FLAGS,
                FILE_OPEN,
                FILE_DIRECTORY_FILE,
                NULL,
                0
            ));

            // While the ioctl is blocking, other threads may still modify the passes strings as
            // they please. Sanitize and keep our own copy of these strings before we exit the
            // __try block.

            constexpr auto Copy = [](UNICODE_STRING& dstString, PUNICODE_STRING src)
            {
                PWSTR dst = (PWSTR)ExAllocatePoolZero(PagedPool,
                    (src->Length + 1) * sizeof(WCHAR), MA_REALITY_TAG);
                if (dst == NULL)
                {
                    return STATUS_NO_MEMORY;
                }
                AUTO_RESOURCE(dst, [](auto p) { ExFreePoolWithTag(p, MA_REALITY_TAG); });

                // YOLO: We're in a __try/__except block.
                memcpy(dst, src->Buffer, src->Length * sizeof(WCHAR));
                dstString.Buffer = dst;
                dstString.Length = src->Length;
                dstString.MaximumLength = src->Length;
                dst = NULL;

                return STATUS_SUCCESS;
            };

            MA_RETURN_IF_FAIL(Copy(strRootDirectory, pUserAttributes->RootDirectory));
            MA_RETURN_IF_FAIL(Copy(strCurrentWorkingDirectory,
                pUserAttributes->CurrentWorkingDirectory));

            constexpr auto CopyList = [](
                _MA_UNICODE_STRING_LIST& dstStringList,
                PUNICODE_STRING src,
                SIZE_T srcCount
            )
            {
                dstStringList.Strings = (PUNICODE_STRING)ExAllocatePoolZero(PagedPool,
                    srcCount * sizeof(UNICODE_STRING), MA_REALITY_TAG);
                if (dstStringList.Strings == NULL)
                {
                    return STATUS_NO_MEMORY;
                }
                dstStringList.Length = srcCount;

                for (SIZE_T i = 0; i < srcCount; ++i)
                {
                    MA_RETURN_IF_FAIL(Copy(dstStringList.Strings[i], &src[i]));
                }

                return STATUS_SUCCESS;
            };

            MA_RETURN_IF_FAIL(CopyList(strListProviderArgs,
                pUserAttributes->ProviderArgs, pUserAttributes->ProviderArgsCount));
            MA_RETURN_IF_FAIL(CopyList(strListArgs,
                pUserAttributes->Args, pUserAttributes->ArgsCount));
            MA_RETURN_IF_FAIL(CopyList(strListEnvironment,
                pUserAttributes->Environment, pUserAttributes->EnvironmentCount));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return STATUS_ACCESS_VIOLATION;
        }

        MA_RETURN_IF_FAIL(ObOpenObjectByPointer(
            PsGetCurrentProcess(),
            OBJ_KERNEL_HANDLE,
            NULL,
            PROCESS_ALL_ACCESS,
            *PsProcessType,
            KernelMode,
            &hdlHostProcess
        ));

        MA_RETURN_IF_FAIL(CdpKernelConsoleAttach(PsGetCurrentProcessId(), &hdlConsole));
        MA_RETURN_IF_FAIL(CdpKernelConsoleOpenHandles(
            hdlConsole,
            &hdlInput,
            &hdlOutput
        ));

        MA_PICO_SESSION_ATTRIBUTES maAttributes
        {
            .Size = sizeof(MA_PICO_SESSION_ATTRIBUTES),
            .HostProcess = hdlHostProcess,
            .Console = hdlConsole,
            .Input = hdlInput,
            .Output = hdlOutput,
            .RootDirectory = hdlRootDirectory,
            .CurrentWorkingDirectory = hdlCurrentWorkingDirectory,
            .ProviderArgsCount = pUserAttributes->ProviderArgsCount,
            .ProviderArgs = strListProviderArgs.Strings,
            .ArgsCount = pUserAttributes->ArgsCount,
            .Args = strListArgs.Strings,
            .EnvironmentCount = pUserAttributes->EnvironmentCount,
            .Environment = strListEnvironment.Strings
        };

        // lxmonika retains control of all its auto resources. The callee is responsible for
        // duplicating.
        return MaStartSession(uProviderIndex, &maAttributes);
    }
    default:
    {
        return STATUS_INVALID_PARAMETER;
    }
    break;
    }
}

extern "C"
NTSTATUS
RlpFileSeek(
    _Inout_ PRL_FILE pFile,
    _In_ INT64 iOffset,
    _In_ INT iWhence,
    _Out_opt_ PINT64 pResultOffset
)
{
    NTSTATUS status = STATUS_SUCCESS;

    ExAcquireFastMutex(&pFile->Lock);

    INT64 newOffset = pFile->Offset;

    switch (iWhence)
    {
    case SEEK_SET:
    {
        newOffset = iOffset;
    }
    break;
    case SEEK_CUR:
    {
        newOffset += iOffset;
    }
    break;
    case SEEK_END:
    {
        newOffset = pFile->Length + iOffset;
    }
    break;
    default:
    {
        status = STATUS_INVALID_PARAMETER;
    }
    }

    if (newOffset < 0)
    {
        status = STATUS_INVALID_PARAMETER;
    }
    else
    {
        pFile->Offset = newOffset;
        if (pResultOffset != NULL)
        {
            *pResultOffset = newOffset;
        }
    }

    ExReleaseFastMutex(&pFile->Lock);

    return status;
}

//
// Reality utilities
//

static
NTSTATUS
RlFileUpdateInformation(
    _Inout_ PRL_FILE pFile
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
        PUNICODE_STRING pProviderName;
        if (NT_SUCCESS(MaGetAllocatedPicoProviderName(pContext->Provider, &pProviderName)))
        {
            Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
                "ProviderName:\t%wZ", pProviderName
            ));
        }
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
                PUNICODE_STRING pParentProviderName;
                if (NT_SUCCESS(MaGetAllocatedPicoProviderName(pContext->Provider,
                    &pParentProviderName)))
                {
                    Write(_snprintf(pFile->Data + pFile->Length, uSizeLeft + 1,
                        "ProviderParentName:\t%wZ", pParentProviderName
                    ));
                }
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

NTSTATUS
RlEscape()
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
    RtlFailFast(RL_DEVICE_CODE);
}
