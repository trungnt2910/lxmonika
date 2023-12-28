#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <Windows.h>
#include <winternl.h>

#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)

using WSTRING = std::basic_string<WCHAR>;

#define IOCTL_MX_METHOD_BUFFERED \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _MX_EXECUTE_INFORMATION
{
    UNICODE_STRING ExecutablePath;
} MX_EXECUTE_INFORMATION, *PMX_EXECUTE_INFORMATION;

template <typename... Args>
void Print(Args&&... arg)
{
    (std::wcout << ... << arg);
    std::wcout << std::endl;
}

#define MX_INFO(...)    Print(L"[INF] ", __FILE__, ":", __LINE__, ": ", __VA_ARGS__)
#define MX_WARN(...)    Print(L"[WRN] ", __FILE__, ":", __LINE__, ": ", __VA_ARGS__)
#define MX_ERROR(...)   Print(L"[ERR] ", __FILE__, ":", __LINE__, ": ", __VA_ARGS__)

#ifdef _DEBUG
#define MX_DEBUG(...)   Print(L"[DBG] ", __FILE__, ":", __LINE__, ": ", __VA_ARGS__)
#else
#define MX_DEBUG(...)
#endif

WSTRING Win32HandleToPath(HANDLE hdlFile, DWORD dwFlags)
{
    SIZE_T dwRequiredSize = GetFinalPathNameByHandleW(hdlFile, NULL, 0, dwFlags);

    WSTRING str(dwRequiredSize * 2, L'\0');

    while (TRUE)
    {
        dwRequiredSize = GetFinalPathNameByHandleW(
            hdlFile, str.data(), (DWORD)str.size(), dwFlags);
        if (dwRequiredSize > str.size())
        {
            str.resize(max(str.size(), dwRequiredSize) * 2);
            continue;
        }

        str.resize(dwRequiredSize);
        str.shrink_to_fit();

        return str;
    }
}

BOOL IntializeRootDirectory(PCSTR pUserParameter, WSTRING* pDosDirectory)
{
    WSTRING strDosPath;
    if (pUserParameter != NULL)
    {
        for (; *pUserParameter != '\0'; ++pUserParameter)
        {
            strDosPath.push_back(*pUserParameter);
        }
    }
    else
    {
        strDosPath = L".";
    }

    HANDLE hdlDir = CreateFileW(
        strDosPath.data(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hdlDir == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    *pDosDirectory = Win32HandleToPath(hdlDir, VOLUME_NAME_DOS) + L"\\";
    CloseHandle(hdlDir);
    return TRUE;
}

class ConsoleCPSetter
{
private:
    UINT m_oldcp;
public:
    ConsoleCPSetter(UINT wCodePageID)
        : m_oldcp(GetConsoleCP())
    {
        SetConsoleCP(wCodePageID);
    }
    ~ConsoleCPSetter()
    {
        SetConsoleCP(m_oldcp);
    }
};

int main(int argc, const char** argv)
{
    auto _ConsoleCP = ConsoleCPSetter(CP_WINUNICODE);

    MX_INFO(L"Monix version 0.0.1 prealpha (compiled ", __DATE__, " ", __TIME__, ")");
    MX_INFO(L"Copyright <C> 2023 Trung Nguyen (trungnt2910)");

    MX_INFO(L"initializing kernel device");

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
        MX_ERROR("Cannot open kernel device: ", (LPVOID)(ULONG_PTR)status);
        return status;
    }

    std::shared_ptr<VOID> _hdlDevice(hdlDevice, [](HANDLE hdl) { NtClose(hdl); });

    MX_INFO(L"initializing system root");

    PCSTR pRootPath = NULL;
    if (argc >= 2)
    {
        pRootPath = argv[1];
    }

    WSTRING strDosRootDir;

    if (!IntializeRootDirectory(pRootPath, &strDosRootDir))
    {
        MX_ERROR("Cannot initialize root directory");
        return STATUS_UNSUCCESSFUL;
    }

    WSTRING strDosBinDir = strDosRootDir + L"bin\\";

    std::wcout << L"Available binaries: " << std::endl;
    bool bFirstFile = true;
    for (const auto& entry : std::filesystem::directory_iterator(strDosBinDir))
    {
        if (entry.is_regular_file())
        {
            if (bFirstFile)
            {
                bFirstFile = false;
            }
            else
            {
                std::wcout << L", ";
            }
            std::wcout << entry.path().filename().wstring();
        }
    }
    std::wcout << L"." << std::endl;

    while (TRUE)
    {
        std::wcout.put(L'>');
        WSTRING strInput;
        std::getline(std::wcin, strInput);
        if (strInput.empty())
        {
            continue;
        }

        HANDLE hdlWin32BinFile = CreateFileW(
            (strDosBinDir + strInput).data(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (hdlWin32BinFile == INVALID_HANDLE_VALUE)
        {
            Print(L"Binary ", strInput, L" not found\n");
            continue;
        }

        WSTRING strNtFilePath = Win32HandleToPath(hdlWin32BinFile, VOLUME_NAME_NT);

        MX_INFO("creating new process");

        MX_EXECUTE_INFORMATION mxExecuteInformation
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
            MX_ERROR("Cannot create process: ", (LPVOID)(ULONG_PTR)status, "\n");
            continue;
        }
    }

    return 0;
}
