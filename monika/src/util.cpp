#include "util.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>

#include <comdef.h>
#include <Windows.h>

#include <Exception.h>

std::wstring_view
UtilGetResourceString(
    int code
)
{
    LPCWSTR lpResult;
    SIZE_T szLen = LoadStringW(NULL, code, (LPWSTR)&lpResult, 0);

    if (szLen == 0)
    {
        return std::wstring_view(L"");
    }

    return std::wstring_view(lpResult, szLen);
}

std::wstring_view
UtilGetErrorMessage(
    HRESULT code
)
{
    static std::unordered_map<HRESULT, std::wstring> cache;

    if (cache.contains(code))
    {
        return cache[code];
    }

    return cache[code] = _com_error(code).ErrorMessage();
}

std::shared_ptr<std::remove_pointer_t<SC_HANDLE>>
UtilGetSharedServiceHandle(
    SC_HANDLE handle,
    bool shouldThrow
)
{
    if (shouldThrow)
    {
        handle = Win32Exception::ThrowIfNull(handle);
    }

    return std::shared_ptr<std::remove_pointer_t<SC_HANDLE>>(handle, CloseServiceHandle);
}

std::shared_ptr<std::remove_pointer_t<HANDLE>>
UtilGetSharedWin32Handle(
    HANDLE hdlObject,
    bool shouldThrow
)
{
    if (shouldThrow)
    {
        hdlObject = Win32Exception::ThrowIfInvalid(hdlObject);
    }

    return std::shared_ptr<std::remove_pointer_t<HANDLE>>(hdlObject, CloseHandle);
}

std::wstring
UtilGetSystemDirectory()
{
    std::wstring result;
    result.resize(result.capacity());

    while (true)
    {
        UINT uSize = Win32Exception::ThrowIfNull(
            GetSystemDirectoryW(result.data(), (UINT)result.size())
        );

        if (uSize > result.size())
        {
            result.resize(uSize);
        }
        else
        {
            result.resize(uSize);
            break;
        }
    }

    return result;
}

std::wstring
UtilGetDriversDirectory()
{
    return (std::filesystem::path(UtilGetSystemDirectory()) / "drivers").wstring();
}

std::wstring
UtilGetExecutableDirectory()
{
    std::wstring executablePathString;
    executablePathString.resize(executablePathString.capacity());

    DWORD dwPathLength = Win32Exception::ThrowIfNull(GetModuleFileNameW(
        NULL, executablePathString.data(), (DWORD)executablePathString.size()
    ));

    while (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        executablePathString.resize(executablePathString.size() * 2);
        dwPathLength = Win32Exception::ThrowIfNull(GetModuleFileNameW(
            NULL, executablePathString.data(), (DWORD)executablePathString.size()
        ));
    }

    executablePathString.resize(dwPathLength);
    return std::filesystem::path(executablePathString).parent_path().wstring();
}

static
std::wstring
UtilGetFinalPathNameByHandle(
    HANDLE handle,
    DWORD dwFlags
)
{
    DWORD dwRequiredSize = Win32Exception::ThrowIfNull(GetFinalPathNameByHandleW(
        handle,
        NULL,
        0,
        dwFlags
    ));

    std::wstring result;
    result.resize(dwRequiredSize);

    while (true)
    {
        dwRequiredSize = Win32Exception::ThrowIfNull(GetFinalPathNameByHandleW(
            handle,
            result.data(),
            (DWORD)result.size(),
            dwFlags
        ));

        if (dwRequiredSize > result.size())
        {
            result.resize(dwRequiredSize);
            continue;
        }
        else
        {
            result.resize(dwRequiredSize);
            break;
        }
    }

    return result;
}

std::filesystem::path
UtilNtToWin32Path(
    const std::filesystem::path& ntPath
)
{
    std::wstring ntPathString = ntPath.wstring();
    UNICODE_STRING ntPathUnicode;
    RtlInitUnicodeString(&ntPathUnicode, ntPathString.c_str());

    OBJECT_ATTRIBUTES objectAttributes;
    InitializeObjectAttributes(
        &objectAttributes,
        &ntPathUnicode,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    IO_STATUS_BLOCK ioStatus;

    HANDLE fileHandle = NULL;
    NTSTATUS status = NtCreateFile(
        &fileHandle,
        SYNCHRONIZE, // At least this must be specified, else we'll get STATUS_INVALID_PARAMETER.
        &objectAttributes,
        &ioStatus,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN,
        0,
        NULL,
        0
    );

    if (!NT_SUCCESS(status))
    {
        throw NTException(status);
    }

    auto sharedHandle = UtilGetSharedWin32Handle(fileHandle);

    return UtilGetFinalPathNameByHandle(sharedHandle.get(), VOLUME_NAME_DOS);
}

std::wstring
UtilWin32ToNtPath(
    const std::filesystem::path& win32Path
)
{
    auto handle = UtilGetSharedWin32Handle(CreateFileW(
        win32Path.wstring().c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    ), false);

    if (handle.get() == INVALID_HANDLE_VALUE)
    {
        handle = UtilGetSharedWin32Handle(CreateFileW(
            win32Path.wstring().c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, // Required for obtaining a directory handle.
            NULL
        ));
    }

    return UtilGetFinalPathNameByHandle(handle.get(), VOLUME_NAME_NT);
}

std::vector<std::wstring>
UtilStringListToVector(
    LPCWSTR pStringList
)
{
    std::vector<std::wstring> result;
    LPCWSTR pCurrent = pStringList;
    while (*pCurrent != L'\0')
    {
        result.emplace_back(pCurrent);
        pCurrent += result.back().size() + 1;
    }
    return result;
}

std::wstring
UtilVectorToStringList(
    const std::vector<std::wstring>& strings
)
{
    std::wstring result;
    for (const std::wstring& str : strings)
    {
        result.reserve(result.size() + str.size() + 1);
        result += str;
        result.push_back('\0');
    }
    return result;
}

bool
UtilCheckCoreDriverName(
    const std::wstring& name
)
{
    static const std::vector<BYTE> winload = ([]()
    {
        std::vector<BYTE> contents;

        std::fstream fin(
            std::filesystem::path(UtilGetSystemDirectory()) / "winload.exe",
            std::ios_base::in | std::ios_base::binary
        );

        if (!fin.is_open())
        {
            // Some newer versions of Windows do not have a winload.exe.
            // Only UEFI boot is supported on these copies.
            fin.open(
                std::filesystem::path(UtilGetSystemDirectory()) / "winload.efi",
                std::ios_base::in | std::ios_base::binary
            );
        }

        std::transform(
            std::istreambuf_iterator<char>(fin),
            std::istreambuf_iterator<char>(),
            std::back_inserter(contents),
            [](char ch) { return (BYTE)ch; }
        );

        return contents;
    })();

    return std::search(
        winload.begin(), winload.end(),
        (BYTE*)&name[0],
        // One byte past the string's null terminator.
        (BYTE*)&name[name.size()] + sizeof(WCHAR)
    ) != winload.end();
}
