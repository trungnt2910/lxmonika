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
    )
{
    AddCommand(_installProviderCommand);
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

    auto service = SvInstallDriver(
        manager,
        SERVICE_START | DELETE,
        // lxmonika.sys should start before any userland process is created,
        // since these process may (indirectly) use lxcore.sys and Pico process services.
        SERVICE_SYSTEM_START,
        MA_SERVICE_NAME,
        MA_SERVICE_DISPLAY_NAME,
        UtilGetResourceString(MA_STRING_MONIKA_JUST_MONIKA),
        fullPath,
        std::nullopt
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
            // This error occurs when a driver fails to start, for example, when an old lxmonika
            // copy prevents the new one from using heuristics to detect offsets.
            // Give the user another chance to reboot in this case.
            return HRESULT_FROM_WIN32(ERROR_SUCCESS_REBOOT_REQUIRED);
        }
        default:
        {
            DeleteService(service.get());
            return HRESULT_FROM_WIN32(code);
        }
        }
    }

    return 0;
}
