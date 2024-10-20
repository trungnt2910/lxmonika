#include "util.h"

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
