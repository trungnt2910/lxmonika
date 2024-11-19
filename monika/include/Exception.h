#pragma once

#include <format>
#include <string>

#include <Windows.h>
#include <winternl.h>

#include "util.h"

class Exception
{
private:
    HRESULT _hresult;
    std::wstring_view _message;
protected:
    Exception(HRESULT hresult, const std::wstring_view& message)
        : _hresult(hresult), _message(message) { }
    Exception(HRESULT hresult)
        : _hresult(hresult) { }
    std::wstring_view& SetMessage(const std::wstring_view& message) { return _message = message; }
public:
    const std::wstring_view& Message() const { return _message; }
    const HRESULT GetHresult() const { return _hresult; }
};

class Win32Exception : public Exception
{
public:
    Win32Exception(DWORD code = GetLastError())
        : Exception(
            HRESULT_FROM_WIN32(code),
            UtilGetErrorMessage(HRESULT_FROM_WIN32(code))
        ) { }

    template <typename T>
    [[nodiscard]]
    static T ThrowIfNull(T result) { return (result != (T)0) ? result : throw Win32Exception(); }

    static void ThrowIfFalse(BOOL result) { if (!result) throw Win32Exception(); }

    template <typename T>
    static void ThrowIfNonZero(T result)
    {
        if (result != ERROR_SUCCESS)
        {
            throw Win32Exception(result);
        }
    }

    static HANDLE ThrowIfInvalid(HANDLE result)
    {
        return (result != INVALID_HANDLE_VALUE) ? result : throw Win32Exception();
    }

    static bool ThrowUnless(DWORD error)
    {
        DWORD code = GetLastError();
        if (code != ERROR_SUCCESS && code != error)
        {
            throw Win32Exception();
        }
        return code != error;
    }
};

class NTException : public Win32Exception
{
public:
    NTException(NTSTATUS status)
        : Win32Exception(RtlNtStatusToDosError(status)) { }
};

class MonikaException : public Exception
{
private:
    std::wstring _message;
public:
    MonikaException(int message, HRESULT hresult = HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR))
        : Exception(hresult, UtilGetResourceString(message)) { }
    template <typename... Args>
    MonikaException(int message, HRESULT hresult, Args&&... args)
        : Exception(hresult),
          _message(std::vformat(UtilGetResourceString(message), std::make_wformat_args(args...)))
    {
        SetMessage(_message);
    }
};
