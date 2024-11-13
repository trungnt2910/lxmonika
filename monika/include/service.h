#pragma once

// service.h
//
// Support functions for driver services.

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <optional>
#include <vector>

#include <Windows.h>

using ServiceHandle = std::shared_ptr<std::remove_pointer_t<SC_HANDLE>>;

ServiceHandle
    SvInstallDriver(
        ServiceHandle manager,
        DWORD dwDesiredAccess,
        DWORD dwStartType,
        const std::wstring& serviceName,
        const std::optional<std::wstring>& displayName,
        const std::optional<std::wstring>& description,
        const std::filesystem::path& binaryPath,
        const std::optional<std::vector<std::wstring>>& dependencies,
        const std::optional<std::wstring>& borrowedServiceName
    );

bool
    SvUninstallDriver(
        ServiceHandle manager,
        const std::wstring& serviceName
    );

bool
    SvIsServiceInstalled(
        ServiceHandle manager,
        const std::wstring& serviceName
    );

bool
    SvIsLxMonikaInstalled(
        ServiceHandle manager
    );

bool
    SvIsLxMonikaRunning(
        ServiceHandle manager
    );

std::span<const ENUM_SERVICE_STATUSW>
    SvGetLxMonikaDependentServices(
        ServiceHandle manager,
        std::vector<char>& buffer
    );

const LPQUERY_SERVICE_CONFIG
    SvQueryServiceConfig(
        ServiceHandle service,
        std::vector<char>& buffer
    );
