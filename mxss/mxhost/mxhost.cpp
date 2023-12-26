#include <iostream>
#include <memory>
#include <string>

#include <Windows.h>
#include <winternl.h>

using WSTRING = std::basic_string<WCHAR>;

#define IOCTL_MX_METHOD_BUFFERED \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _MX_EXECUTE_INFORMATION
{
    UNICODE_STRING ExecutablePath;
} MX_EXECUTE_INFORMATION, *PMX_EXECUTE_INFORMATION;

DWORD Fail(NTSTATUS status, LPCSTR pMessage)
{
    std::cerr << pMessage << ": " << std::hex << status << std::dec << std::endl;
    return status;
}

WSTRING GetCurrentDirectory()
{
    SIZE_T dwRequiredSize = GetCurrentDirectoryW(0, NULL);

    WSTRING str(dwRequiredSize * 2, L'\0');

    while (TRUE)
    {
        dwRequiredSize = GetCurrentDirectoryW((DWORD)str.size(), str.data());
        if (dwRequiredSize > str.size())
        {
            str.resize(max(str.size(), dwRequiredSize) * 2);
            continue;
        }

        str.resize(dwRequiredSize);

        if (str.back() != L'\\')
        {
            str.push_back(L'\\');
        }

        str.shrink_to_fit();
        return str;
    }
}

int main()
{
    SetConsoleCP(CP_WINUNICODE);

    UNICODE_STRING strDevicePath;
    RtlInitUnicodeString(&strDevicePath, L"\\Device\\mxss");

    OBJECT_ATTRIBUTES objAttributes;
    InitializeObjectAttributes(
        &objAttributes,
        &strDevicePath,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    IO_STATUS_BLOCK ioStatus;

    HANDLE hdlDevice = NULL;
    NTSTATUS status = NtCreateFile(
        &hdlDevice,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        &objAttributes,
        &ioStatus,
        NULL,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_CREATE,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
    );

    if (!NT_SUCCESS(status))
    {
        return Fail(status, "Failed to open mxss device");
    }

    std::shared_ptr<VOID> _(hdlDevice, [](HANDLE hdl) { NtClose(hdl); });

    WSTRING strNtFilePath = L"\\??\\Global\\";
    strNtFilePath += GetCurrentDirectory();
    strNtFilePath += L"bin\\hello";

    MX_EXECUTE_INFORMATION mxExecuteInformation = MX_EXECUTE_INFORMATION
    {
        .ExecutablePath = UNICODE_STRING
        {
            .Length = (WORD)(strNtFilePath.size() * sizeof(WCHAR)),
            .MaximumLength = (WORD)(strNtFilePath.size() * sizeof(WCHAR)),
            .Buffer = strNtFilePath.data()
        }
    };
    NTSTATUS statusExecute = 0;

    status = NtDeviceIoControlFile(
        hdlDevice,
        NULL,
        NULL,
        NULL,
        &ioStatus,
        IOCTL_MX_METHOD_BUFFERED,
        &mxExecuteInformation,
        sizeof(mxExecuteInformation),
        &statusExecute,
        sizeof(statusExecute)
    );

    if (!NT_SUCCESS(status))
    {
        return Fail(status, "Failed to perform IO operation");
    }

    std::wcout
        << L"Executed program \"" << strNtFilePath.c_str() << "\" "
        << L"with status code " << std::hex << statusExecute << std::dec
        << "." << std::endl;

    return 0;
}
