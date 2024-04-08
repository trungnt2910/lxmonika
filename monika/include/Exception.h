#pragma once

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
};

class NTException : public Win32Exception
{
public:
    NTException(NTSTATUS status)
        : Win32Exception(RtlNtStatusToDosError(status)) { }
};

class MonikaException : public Exception
{
public:
    MonikaException(int message, HRESULT hresult = HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR))
        : Exception(hresult, UtilGetResourceString(message)) { }
};
