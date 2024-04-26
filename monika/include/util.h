#pragma once

// util.h
//
// Utility functions.

#include <memory>
#include <string_view>

#include <Windows.h>
#include <winsvc.h>

std::wstring_view
    UtilGetResourceString(
        int code
    );

std::wstring_view
    UtilGetErrorMessage(
        HRESULT code
    );

std::shared_ptr<std::remove_pointer_t<SC_HANDLE>>
    UtilGetSharedServiceHandle(
        SC_HANDLE handle,
        bool shouldThrow = true
    );

std::wstring
    UtilGetSystemDirectory();
