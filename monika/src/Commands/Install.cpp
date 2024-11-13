#include "Commands/Install.h"

#include <filesystem>
#include <iostream>
#include <memory>

#include <Windows.h>
#include <winsvc.h>

#include "constants.h"
#include "resource.h"
#include "service.h"
#include "util.h"

#include "Exception.h"
#include "Parameter.h"
#include "Switch.h"

#include "Commands/InstallProvider.h"

// Services to "borrow", sorted by preference.

static constexpr PCWSTR MaKnownCoreServices[] =
{
    // Known Pico services. Whitelisted in some Windows versions.
    L"ADSS", L"LXSS",
    // Crytographic Number Generator driver.
    // Supported by lxmonika and whitelisted in all Windows versions.
    // However, failure to load could cause a bootloop.
    L"CNG"
};

static constexpr PCWSTR MaKnownCorePicoServices[] =
{
    L"ADSS", L"LXSS"
};

Install::Install(const CommandBase* parentCommand)
  : Command(
        MA_STRING_INSTALL_COMMAND_NAME,
        MA_STRING_INSTALL_COMMAND_DESCRIPTION,
        _rest,
        parentCommand
    ),
    _installProviderCommand(this),
    _rest(
        -1, -1, MA_STRING_INSTALL_COMMAND_ARGUMENT_DESCRIPTION,
        DriverPathParameter, _path, true
    ),
    _forceSwitch(
        MA_STRING_INSTALL_SWITCH_FORCE_NAME, -1,
        MA_STRING_INSTALL_SWITCH_FORCE_DESCRIPTION,
        NullParameter,
        _force,
        true
    )
{
    AddCommand(_installProviderCommand);

    AddSwitch(_forceSwitch);
}

int
Install::Execute() const
{
    std::filesystem::path fullPath;

    if (_path.has_value())
    {
        fullPath = std::filesystem::canonical(_path.value());
    }
    else
    {
        std::wstring executablePathString;
        executablePathString.resize(executablePathString.capacity());

        DWORD dwPathLength = Win32Exception::ThrowIfNull(GetModuleFileNameW(
            NULL, executablePathString.data(), (DWORD)executablePathString.size()
        ));

        while (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
            executablePathString.resize(executablePathString.size() * 2);
            dwPathLength = Win32Exception::ThrowIfNull(GetModuleFileNameW(
                NULL, executablePathString.data(), (DWORD)executablePathString.size()
            ));
        }

        executablePathString.resize(dwPathLength);

        std::error_code ec;

        fullPath = std::filesystem::canonical(
            std::filesystem::path(executablePathString).parent_path()
            / MA_SERVICE_DRIVER_NAME,
            ec
        );

        if (ec)
        {
            throw MonikaException(MA_STRING_EXCEPTION_DEFAULT_DRIVER_PATH_NOT_FOUND,
                HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }
    }

    auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
        NULL, NULL, SC_MANAGER_CREATE_SERVICE | SC_MANAGER_ENUMERATE_SERVICE
    ));

    if (SvIsLxMonikaInstalled(manager))
    {
        throw MonikaException(MA_STRING_EXCEPTION_LXMONIKA_ALREADY_INSTALLED);
    }

    std::optional<std::wstring> borrowedService;

    // First loop: Check for running Pico providers to intercept.
    for (PCWSTR pService : MaKnownCorePicoServices)
    {
        if (SvIsServiceInstalled(manager, pService) && UtilCheckCoreDriverName(pService))
        {
            borrowedService = pService;
            break;
        }
    }

    if (!borrowedService.has_value())
    {
        // Second loop: Take any available core driver services.
        for (PCWSTR pService : MaKnownCoreServices)
        {
            if (UtilCheckCoreDriverName(pService))
            {
                borrowedService = pService;
                break;
            }
        }
    }

    auto service = SvInstallDriver(
        manager,
        SERVICE_START | DELETE,
        // lxmonika.sys should start before any userland process is created,
        // since these process may (indirectly) use lxcore.sys and Pico process services.
        SERVICE_BOOT_START,
        MA_SERVICE_NAME,
        MA_SERVICE_DISPLAY_NAME,
        std::wstring(UtilGetResourceString(MA_STRING_MONIKA_JUST_MONIKA)),
        fullPath,
        std::nullopt,
        borrowedService
    );

    if (!StartServiceW(
        service.get(),
        0,
        NULL
    ))
    {
        int code = GetLastError();
        switch (code)
        {
            case ERROR_NOT_FOUND:
            {
                // This error occurs when a driver fails to start, for example, when an old
                // lxmonika copy prevents the new one from using heuristics to detect offsets.
                // Give the user another chance to reboot in this case.
                return HRESULT_FROM_WIN32(ERROR_SUCCESS_REBOOT_REQUIRED);
            }
            case ERROR_WRITE_PROTECT:
            {
                // Recent versions of lxmonika returns STATUS_TOO_LATE on their first run
                // since they could not access Pico provider registration functions.
                // This translates to a Win32 "ERROR_WRITE_PROTECT".
                return HRESULT_FROM_WIN32(ERROR_SUCCESS_REBOOT_REQUIRED);
            }
            default:
            {
                SvUninstallDriver(manager, MA_SERVICE_NAME);
                return HRESULT_FROM_WIN32(code);
            }
        }
    }

    return 0;
}
