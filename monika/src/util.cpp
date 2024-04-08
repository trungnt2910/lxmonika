#include "util.h"

#include <unordered_map>

#include <comdef.h>
#include <Windows.h>

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
