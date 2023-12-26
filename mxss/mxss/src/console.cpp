#include "console.h"

#include <intsafe.h>

extern "C"
NTSTATUS
CoAttachKernelConsole(
    _In_ HANDLE hdlOwnerProcess,
    _Out_ PHANDLE pHdlConsole
)
{
    // Offset               Data
    // 0                    (DWORD)1356
    // 5                    (CHAR)6
    // 6                    (CHAR)'<' // 60, 0x3C
    // 7                    (CHAR)5
    // 8                    "server"
    // 43                   (DWORD)PsGetProcessId(PsInitialSystemProcess)
    // 49                   (WORD)MIN(520, COUNT_CHAR_BYTES(L"Command Prompt"))
    // 51                   L"Command Prompt"
    // 573                  (WORD)MIN(254, COUNT_CHAR_BYTES(L"System"))
    // 575                  L"System"
    // 831                  (WORD)MIN(520, COUNT_CHAR_BYTES(RtlGetNtSystemRoot()))
    // 833                  RtlGetNtSystemRoot()
    // 1356                 (DWORD)0
    // 1361                 (CHAR)6
    // 1362                 (WORD)8
    // 1364                 "attach"
    // 1371                 (DWORD)ownerProcessId
    // 1380                 END

    CHAR pAttributes[1380];
    memset(pAttributes, 0, sizeof(pAttributes));

    CONST DWORD uMagic1 = 1356;
    CONST CHAR pCommand1[] = "server";
    CONST DWORD uServerProcessId = (DWORD)(ULONG_PTR)PsGetProcessId(PsInitialSystemProcess);
    CONST WCHAR pInfo1_1[] = L"Command Prompt";
    CONST WORD pInfoBytes1_1 = min(522, sizeof(pInfo1_1)) - sizeof(L'\0');
    CONST WCHAR pInfo1_2[] = L"System";
    CONST WORD pInfoBytes1_2 = min(256, sizeof(pInfo1_2)) - sizeof(L'\0');
    CONST PCWSTR pInfo1_3 = RtlGetNtSystemRoot();
    CONST WORD pInfoBytes1_3 = (WORD)
        (min(522, (wcslen(pInfo1_3) + 1) * sizeof(WCHAR)) - sizeof(L'\0'));

    memcpy(pAttributes + 0, &uMagic1, sizeof(uMagic1));
    pAttributes[5] = 0x6;
    pAttributes[6] = 0x3C;
    pAttributes[7] = 0x5;
    memcpy(pAttributes + 8,     pCommand1, sizeof(pCommand1));
    memcpy(pAttributes + 43,    &uServerProcessId, sizeof(uServerProcessId));
    memcpy(pAttributes + 49,    &pInfoBytes1_1, sizeof(pInfoBytes1_1));
    memcpy(pAttributes + 51,    pInfo1_1, pInfoBytes1_1);
    memcpy(pAttributes + 573,   &pInfoBytes1_2, sizeof(pInfoBytes1_2));
    memcpy(pAttributes + 575,   pInfo1_2, pInfoBytes1_2);
    memcpy(pAttributes + 831,   &pInfoBytes1_3, sizeof(pInfoBytes1_3));
    memcpy(pAttributes + 833,   pInfo1_3, pInfoBytes1_3);

    CONST DWORD uMagic2 = 0;
    CONST CHAR pCommand2[] = "attach";
    CONST DWORD uOwnerProcessId = (DWORD)(ULONG_PTR)hdlOwnerProcess;

    memcpy(pAttributes + uMagic1,       &uMagic2, sizeof(uMagic2));
    pAttributes[uMagic1 + 5] = 0x6;
    pAttributes[uMagic1 + 6] = 0x8;
    memcpy(pAttributes + uMagic1 + 8,   pCommand2, sizeof(pCommand2));
    memcpy(pAttributes + uMagic1 + 15,  &uOwnerProcessId, sizeof(uOwnerProcessId));

    // At this point I really need to see a therapist.

    UNICODE_STRING strDevicePath = RTL_CONSTANT_STRING(L"\\Device\\ConDrv\\KernelConnect");
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK ioStatus;

    InitializeObjectAttributes(
        &objectAttributes,
        &strDevicePath,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    return ZwCreateFile(
        pHdlConsole,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        &objectAttributes,
        &ioStatus,
        NULL,
        0,
        FILE_SHARE_VALID_FLAGS,
        FILE_CREATE,
        FILE_SYNCHRONOUS_IO_NONALERT,
        pAttributes,
        sizeof(pAttributes)
    );
}

extern "C"
NTSTATUS
CoOpenStandardHandles(
    _In_ HANDLE hdlConsole,
    _Out_opt_ PHANDLE pHdlInput,
    _Out_opt_ PHANDLE pHdlOutput
)
{
    NTSTATUS status;
    IO_STATUS_BLOCK ioStatus;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING strName;

    HANDLE hdlOpenedInput = NULL;
    HANDLE hdlOpenedOutput = NULL;

    InitializeObjectAttributes(
        &objectAttributes,
        &strName,
        OBJ_KERNEL_HANDLE | OBJ_INHERIT,
        hdlConsole,
        NULL
    );

    if (pHdlInput != NULL)
    {
        strName = RTL_CONSTANT_STRING(L"\\Input");

        status = ZwCreateFile(
            &hdlOpenedInput,
            FILE_GENERIC_READ | FILE_GENERIC_WRITE,
            &objectAttributes,
            &ioStatus,
            NULL,
            0,
            FILE_SHARE_VALID_FLAGS,
            FILE_CREATE,
            FILE_SYNCHRONOUS_IO_NONALERT,
            NULL,
            0
        );

        if (!NT_SUCCESS(status))
        {
            goto fail;
        }
    }

    if (pHdlOutput != NULL)
    {
        strName = RTL_CONSTANT_STRING(L"\\Output");

        status = ZwCreateFile(
            &hdlOpenedOutput,
            FILE_GENERIC_READ | FILE_GENERIC_WRITE,
            &objectAttributes,
            &ioStatus,
            NULL,
            0,
            FILE_SHARE_VALID_FLAGS,
            FILE_CREATE,
            FILE_SYNCHRONOUS_IO_NONALERT,
            NULL,
            0
        );

        if (!NT_SUCCESS(status))
        {
            goto fail;
        }
    }

    if (pHdlInput != NULL)
    {
        *pHdlInput = hdlOpenedInput;
    }

    if (pHdlOutput != NULL)
    {
        *pHdlOutput = hdlOpenedOutput;
    }

    return STATUS_SUCCESS;
fail:
    if (hdlOpenedInput != NULL)
    {
        ZwClose(hdlOpenedInput);
    }
    if (hdlOpenedOutput != NULL)
    {
        ZwClose(hdlOpenedOutput);
    }
    return status;
}

extern "C"
DECLSPEC_DEPRECATED
NTSTATUS
CoOpenNewestWslHandle(
    _Out_ PHANDLE pHdlWsl
)
{
    typedef struct _SYSTEM_THREADS {
        LARGE_INTEGER  KernelTime;
        LARGE_INTEGER  UserTime;
        LARGE_INTEGER  CreateTime;
        ULONG          WaitTime;
        PVOID          StartAddress;
        CLIENT_ID      ClientId;
        KPRIORITY      Priority;
        KPRIORITY      BasePriority;
        ULONG          ContextSwitchCount;
        LONG           State;
        LONG           WaitReason;
    } SYSTEM_THREADS, *PSYSTEM_THREADS;

    typedef struct _SYSTEM_PROCESSES {
        ULONG            NextEntryDelta;
        ULONG            ThreadCount;
        ULONG            Reserved1[6];
        LARGE_INTEGER    CreateTime;
        LARGE_INTEGER    UserTime;
        LARGE_INTEGER    KernelTime;
        UNICODE_STRING   ProcessName;
        KPRIORITY        BasePriority;
        SIZE_T           ProcessId;
        SIZE_T           InheritedFromProcessId;
        ULONG            HandleCount;
        ULONG            Reserved2[2];
        VM_COUNTERS      VmCounters;
        IO_COUNTERS      IoCounters;
        SYSTEM_THREADS   Threads[1];
    } SYSTEM_PROCESSES, *PSYSTEM_PROCESSES;

    typedef enum _SYSTEM_INFORMATION_CLASS {
        SystemProcessInformation = 5
    } SYSTEM_INFORMATION_CLASS;

    __declspec(dllimport)
        NTSTATUS
        ZwQuerySystemInformation(
            _In_        SYSTEM_INFORMATION_CLASS SystemInformationClass,
            _In_opt_    PVOID SystemInformation,
            _In_        ULONG SystemInformationLength,
            _Out_       PULONG ReturnLength
        );

    NTSTATUS status = STATUS_SUCCESS;
    ULONG uBufferSize = 0;
    PCHAR pBuffer = NULL;
    LARGE_INTEGER liLargestCreateTime =
    {
        .QuadPart = 0
    };
    HANDLE hdlResult = NULL;

    status = ZwQuerySystemInformation(
        SystemProcessInformation, NULL, 0, &uBufferSize);

    uBufferSize *= 2;

    pBuffer = (PCHAR)ExAllocatePoolZero(PagedPool, uBufferSize, '  xM');

    if (pBuffer == NULL)
    {
        status = STATUS_NO_MEMORY;
        goto end;
    }

    status = ZwQuerySystemInformation(
        SystemProcessInformation, pBuffer, uBufferSize, &uBufferSize);

    if (!NT_SUCCESS(status))
    {
        goto end;
    }

    for (PSYSTEM_PROCESSES pSystemProcess = (PSYSTEM_PROCESSES)pBuffer;
        pSystemProcess != NULL;
        pSystemProcess = (PSYSTEM_PROCESSES)((pSystemProcess->NextEntryDelta != 0) ?
            ((PCHAR)pSystemProcess + pSystemProcess->NextEntryDelta) : NULL)
        )
    {
        if (pSystemProcess->ProcessName.Length == 0)
        {
            continue;
        }

        if (wcscmp(pSystemProcess->ProcessName.Buffer, L"wsl.exe") != 0)
        {
            continue;
        }

        if (pSystemProcess->CreateTime.QuadPart > liLargestCreateTime.QuadPart)
        {
            liLargestCreateTime = pSystemProcess->CreateTime;
            hdlResult = (HANDLE)pSystemProcess->ProcessId;
        }
    }

    if (hdlResult != NULL)
    {
        *pHdlWsl = hdlResult;
        status = STATUS_SUCCESS;
    }
    else
    {
        status = STATUS_NOT_FOUND;
    }

end:
    if (pBuffer != NULL)
    {
        ExFreePoolWithTag(pBuffer, '  xM');
    }

    return status;
}
