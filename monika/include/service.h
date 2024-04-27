#pragma once

// service.h
//
// Support functions for driver services.

#include <filesystem>
#include <memory>
#include <string>
#include <optional>
#include <vector>

#include <Windows.h>

using ServiceHandle = std::shared_ptr<std::remove_pointer_t<SC_HANDLE>>;

ServiceHandle
    SvInstallDriver(
        ServiceHandle manager,
        DWORD dwDesiredAccess,
        const std::wstring_view& serviceName,
        const std::optional<std::wstring_view>& displayName,
        const std::optional<std::wstring_view>& description,
        const std::filesystem::path& binaryPath,
        const std::optional<std::vector<std::wstring>>& dependencies
    );

bool
    SvIsLxMonikaInstalled(
        ServiceHandle manager
    );
