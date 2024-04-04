#include "condrv.h"

#include "monika.h"

#include "AutoResource.h"

//
// ConDrv support functions.
// TODO: Export these potentially useful functions as MaUtil*?
//

extern "C"
NTSTATUS
CdpKernelConsoleAttach(
    _In_ HANDLE hdlOwnerProcess,
    _Out_ PHANDLE pHdlConsole
)
{
    // Make things work better with macros
    using CD_SERVER_INFORMATION = CONSOLE_SERVER_MSG;

#define INITIALIZE_STRING(name, value)                                                          \
    .name##Length = min(sizeof(CD_SERVER_INFORMATION::name), sizeof(value))                     \
                        - sizeof((decltype(*value))'\0'),                                       \
    .name = value

    CD_SERVER_INFORMATION cdServerInformation =
    {
        .ProcessGroupId = (ULONG)(ULONG_PTR)PsGetProcessId(PsInitialSystemProcess),
        INITIALIZE_STRING(Title, L"Command Prompt"),
        INITIALIZE_STRING(ApplicationName, L"System")
    };

#undef INITIALIZE_STRING

    PCWSTR pCurrentDirectory = RtlGetNtSystemRoot();
    USHORT uCurrentDirectoryLength = (USHORT)
        min(
            sizeof(CD_SERVER_INFORMATION::CurrentDirectory),
            (wcslen(pCurrentDirectory) + 1) * sizeof(WCHAR)
        ) - sizeof(L'\0');

    cdServerInformation.CurrentDirectoryLength = uCurrentDirectoryLength;
    memcpy(cdServerInformation.CurrentDirectory, pCurrentDirectory,
        uCurrentDirectoryLength + sizeof(L'\0'));

    CD_ATTACH_INFORMATION cdAttachInformation =
    {
        .ProcessId = hdlOwnerProcess
    };

#define EA_ENTRY_SIZE(type)                                                                     \
    ALIGN_UP_BY(                                                                                \
        /* The header before the name. */                                                       \
        FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) +                                        \
        /* The ASCII name itself, including the terminating null character. */                  \
        sizeof(CD_##type##_EA_NAME) +                                                           \
        /* The data structure, unaligned. */                                                    \
        sizeof(CD_##type##_INFORMATION),                                                        \
        /* Align everything up for the next EA header. */                                       \
        alignof(FILE_FULL_EA_INFORMATION)                                                       \
    )

    __declspec(align(alignof(FILE_FULL_EA_INFORMATION)))
    CHAR pAttributes[EA_ENTRY_SIZE(SERVER) + EA_ENTRY_SIZE(ATTACH)];
    memset(pAttributes, 0, sizeof(pAttributes));

    PFILE_FULL_EA_INFORMATION pServerEa = (PFILE_FULL_EA_INFORMATION)pAttributes;
    pServerEa->NextEntryOffset = EA_ENTRY_SIZE(SERVER);
    pServerEa->Flags = 0;
    pServerEa->EaNameLength = sizeof(CD_SERVER_EA_NAME) - sizeof('\0');
    pServerEa->EaValueLength = sizeof(CD_SERVER_INFORMATION);
    memcpy(pServerEa->EaName, CD_SERVER_EA_NAME, sizeof(CD_SERVER_EA_NAME));
    memcpy(pServerEa->EaName + sizeof(CD_SERVER_EA_NAME), &cdServerInformation,
        sizeof(CD_SERVER_INFORMATION));

    PFILE_FULL_EA_INFORMATION pAttachEa = (PFILE_FULL_EA_INFORMATION)
        (pAttributes + EA_ENTRY_SIZE(SERVER));
    pAttachEa->NextEntryOffset = 0;
    pAttachEa->Flags = 0;
    pAttachEa->EaNameLength = sizeof(CD_ATTACH_EA_NAME) - sizeof('\0');
    pAttachEa->EaValueLength = sizeof(CD_ATTACH_INFORMATION);
    memcpy(pAttachEa->EaName, CD_ATTACH_EA_NAME, sizeof(CD_ATTACH_EA_NAME));
    memcpy(pAttachEa->EaName + sizeof(CD_ATTACH_EA_NAME), &cdAttachInformation,
        sizeof(CD_ATTACH_INFORMATION));

#undef EA_ENTRY_SIZE

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
CdpKernelConsoleOpenHandles(
    _In_ HANDLE hdlConsole,
    _Out_opt_ PHANDLE pHdlInput,
    _Out_opt_ PHANDLE pHdlOutput
)
{
    IO_STATUS_BLOCK ioStatus;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING strName;

    HANDLE hdlOpenedInput = NULL;
    AUTO_RESOURCE(hdlOpenedInput, ZwClose);
    HANDLE hdlOpenedOutput = NULL;
    AUTO_RESOURCE(hdlOpenedOutput, ZwClose);

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

        MA_RETURN_IF_FAIL(ZwCreateFile(
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
        ));
    }

    if (pHdlOutput != NULL)
    {
        strName = RTL_CONSTANT_STRING(L"\\Output");

        MA_RETURN_IF_FAIL(ZwCreateFile(
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
        ));
    }

    if (pHdlInput != NULL)
    {
        *pHdlInput = hdlOpenedInput;
        hdlOpenedInput = NULL;
    }

    if (pHdlOutput != NULL)
    {
        *pHdlOutput = hdlOpenedOutput;
        hdlOpenedOutput = NULL;
    }

    return STATUS_SUCCESS;
}
