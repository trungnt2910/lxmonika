#pragma once

// util.h
//
// Utility functions.

#include <filesystem>
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

std::shared_ptr<std::remove_pointer_t<HANDLE>>
    UtilGetSharedWin32Handle(
        HANDLE hdlObject,
        bool shouldThrow = true
    );

std::wstring
    UtilGetSystemDirectory();

std::filesystem::path
    UtilNtToWin32Path(
        const std::filesystem::path& ntPath
    );

std::wstring
    UtilWin32ToNtPath(
        const std::filesystem::path& win32Path
    );

std::vector<std::wstring>
    UtilStringListToVector(
        LPCWSTR pStringList
    );

std::wstring
    UtilVectorToStringList(
        const std::vector<std::wstring>& strings
    );

bool
    UtilCheckCoreDriverName(
        const std::wstring& name
    );
