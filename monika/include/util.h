#pragma once

// util.h
//
// Utility functions.

#include <string_view>

#include <Windows.h>

std::wstring_view
    UtilGetResourceString(
        int code
    );

std::wstring_view
    UtilGetErrorMessage(
        HRESULT code
    );
