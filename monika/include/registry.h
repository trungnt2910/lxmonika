#pragma once

// registry.h
//
// Functions for configuring the system through the Registry

#include <any>
#include <string>
#include <unordered_map>

#include <Windows.h>

using RegistryValues = std::unordered_map<std::wstring, std::any>;

bool
    RegIsTestSigningEnabled();

RegistryValues
    RegQueryRegistryKey(
        const std::wstring& path
    );

void
    RegSetRegistryKey(
        const std::wstring& path,
        const RegistryValues& contents
    );

void
    RegClearRegistryKey(
        const std::wstring& path
    );

class ExpandString : public std::wstring
{
    // For RTTI purposes only.
};
