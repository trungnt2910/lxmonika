#include "Commands/Install.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include <Windows.h>
#include <winsvc.h>

#include "constants.h"
#include "registry.h"
#include "resource.h"
#include "service.h"
#include "util.h"

#include "Exception.h"
#include "Parameter.h"
#include "Switch.h"
#include "Transaction.h"

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
    _coreSwitch(
        MA_STRING_INSTALL_SWITCH_CORE_NAME, -1,
        MA_STRING_INSTALL_SWITCH_CORE_DESCRIPTION,
        DriverPathParameter,
        _core,
        true
    ),
    _borrowSwitch(
        MA_STRING_INSTALL_SWITCH_BORROW_NAME, -1,
        MA_STRING_INSTALL_SWITCH_BORROW_DESCRIPTION,
        StringParameter,
        _borrow,
        true
    ),
    _enableLateRegistrationSwitch(
        MA_STRING_INSTALL_SWITCH_ENABLE_LATE_REGISTRATION_NAME, -1,
        MA_STRING_INSTALL_SWITCH_ENABLE_LATE_REGISTRATION_DESCRIPTION,
        NullParameter,
        _enableLateRegistration,
        true
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

    AddSwitch(_coreSwitch);
    AddSwitch(_borrowSwitch);
    AddSwitch(_enableLateRegistrationSwitch);
    AddSwitch(_forceSwitch);
}

int
Install::Execute() const
{
    // Should be either SUCCESS or SUCCESS_REBOOT_REQUIRED.
    // Error codes should be thrown as exceptions.
    HRESULT hresult = HRESULT_FROM_WIN32(ERROR_SUCCESS);

    //
    // Build driver installation path
    //

    std::filesystem::path fullPath;

    if (_path.has_value())
    {
        fullPath = std::filesystem::canonical(_path.value());
    }
    else
    {
        std::error_code ec;

        fullPath = std::filesystem::canonical(
            std::filesystem::path(UtilGetExecutableDirectory()) / MA_SERVICE_DRIVER_NAME,
            ec
        );

        if (ec)
        {
            throw MonikaException(MA_STRING_EXCEPTION_DEFAULT_DRIVER_PATH_NOT_FOUND,
                HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }
    }

    //
    // Check for existing installations
    //

    auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
        NULL, NULL, SC_MANAGER_CREATE_SERVICE | SC_MANAGER_ENUMERATE_SERVICE
    ));

    if (SvIsLxMonikaInstalled(manager))
    {
        throw MonikaException(MA_STRING_EXCEPTION_LXMONIKA_ALREADY_INSTALLED);
    }

    //
    // Test signing
    //

    if (!_force && !RegIsTestSigningEnabled())
    {
        throw MonikaException(MA_STRING_EXCEPTION_TEST_SIGNING_NOT_ENABLED);
    }

    //
    // Core check
    //

    std::filesystem::path installedCorePath =
        std::filesystem::path(UtilGetDriversDirectory()) / MA_CORE_DRIVER_NAME;

    std::filesystem::path coreDriverPath;
    bool coreDriverPerformInstall = false;

    // If lxcore.sys is not already installed.
    if (!std::filesystem::exists(installedCorePath))
    {
        coreDriverPerformInstall = true;

        if (_core.has_value())
        {
            // Guaranteed to be valid by DriverPathParameter
            coreDriverPath = _core.value();
        }
        else
        {
            coreDriverPath = std::filesystem::weakly_canonical(
                std::filesystem::path(UtilGetExecutableDirectory()) / MA_CORE_DRIVER_NAME
            );
        }
    }

    if (coreDriverPerformInstall
        && !std::filesystem::exists(coreDriverPath))
    {
        if (!_force)
        {
            // lxcore.sys is not already installed and the user has not specified a path,
            // and the current installation does not come with one.
            throw MonikaException(MA_STRING_EXCEPTION_CORE_DRIVER_NOT_FOUND,
                HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }
        else
        {
            // The user doesn't care about lxcore.sys and is OK with a potential bootloop.
            coreDriverPerformInstall = false;
        }
    }

    Transaction coreDriverCopy(
        [&]()
        {
            if (coreDriverPerformInstall)
            {
                Win32Exception::ThrowIfFalse(CopyFileW(
                    coreDriverPath.wstring().c_str(),
                    installedCorePath.wstring().c_str(),
                    true
                ));

                std::ofstream coreDriverCopiedDataStream;
                coreDriverCopiedDataStream.open(
                    installedCorePath.wstring() + L":" MA_CORE_COPIED_STREAM_NAME L":$DATA",
                    std::ios_base::out | std::ios_base::binary
                );
                coreDriverCopiedDataStream.put(1);
            }
        },
        [&]()
        {
            if (coreDriverPerformInstall)
            {
                std::filesystem::remove(installedCorePath);
            }
        }
    );

    //
    // Service borrowing
    //

    std::optional<std::wstring> borrowedService = _borrow;

    if (!borrowedService.has_value())
    {
        // First loop: Check for running Pico providers to intercept.
        for (PCWSTR pService : MaKnownCorePicoServices)
        {
            if (SvIsServiceInstalled(manager, pService) && UtilCheckCoreDriverName(pService))
            {
                borrowedService = pService;
                break;
            }
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

    if (!_force)
    {
        if (!borrowedService.has_value())
        {
            if (!_enableLateRegistration)
            {
                // No borrowable core service found on the system.
                throw MonikaException(
                    MA_STRING_EXCEPTION_BORROWED_SERVICE_NOT_FOUND,
                    HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_FOUND)
                );
            }
        }
        else if (!UtilCheckCoreDriverName(borrowedService.value()))
        {
            // The specified service is not a whitelisted core driver service.
            throw MonikaException(
                MA_STRING_EXCEPTION_BORROWED_SERVICE_INVALID,
                HRESULT_FROM_WIN32(ERROR_SERVICE_NOT_FOUND)
            );
        }
    }

    //
    // Driver installation
    //

    ServiceHandle service;

    Transaction driverInstall(
        [&]()
        {
            service = SvInstallDriver(
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
        },
        [&]()
        {
            SvUninstallDriver(manager, MA_SERVICE_NAME);
        }
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
            // This error occurs when a driver fails to start, for example, when an old
            // lxmonika copy prevents the new one from using heuristics to detect offsets.
            // Give the user another chance to reboot in this case.
            case ERROR_NOT_FOUND:
            // Recent versions of lxmonika return STATUS_TOO_LATE on their first run
            // since they could not access Pico provider registration functions.
            // This translates to a Win32 "ERROR_WRITE_PROTECT".
            case ERROR_WRITE_PROTECT:
                hresult = ERROR_SUCCESS_REBOOT_REQUIRED;
            break;
            default:
                throw Win32Exception(code);
        }
    }

    //
    // Late registration
    //

    Transaction lateRegistrationRegistryKeyAdd(
        [&]()
        {
            if (_enableLateRegistration)
            {
                SvSetServiceParameters(
                    MA_SERVICE_NAME,
                    {
                        {
                            std::wstring(MA_SERVICE_REGISTRY_ENABLE_LATE_REGISTRATION),
                            (DWORD)1
                        }
                    }
                );
            }
        },
        [&]()
        {
            if (_enableLateRegistration)
            {
                SvClearServiceParameters(MA_SERVICE_NAME);
            }
        }
    );

    coreDriverCopy.Commit();
    driverInstall.Commit();
    lateRegistrationRegistryKeyAdd.Commit();

    return hresult;
}
