#include "Commands/Install.h"

#include <filesystem>
#include <iostream>
#include <memory>

#include <Windows.h>
#include <winsvc.h>

#include "constants.h"
#include "resource.h"
#include "util.h"

#include "Exception.h"
#include "Parameter.h"
#include "Switch.h"

Install::Install(const CommandBase* parentCommand)
  : Command(
        MA_STRING_INSTALL_COMMAND_NAME,
        MA_STRING_INSTALL_COMMAND_DESCRIPTION,
        _rest,
        parentCommand
    ),
    _rest(
        -1, -1, MA_STRING_INSTALL_COMMAND_ARGUMENT_DESCRIPTION,
        DriverPathParameter, _path, true
    )
{
    // Currently no-op.
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
            / MA_SERVICE_DEFAULT_DRIVER_NAME,
            ec
        );

        if (ec)
        {
            throw MonikaException(MA_STRING_EXCEPTION_DEFAULT_DRIVER_PATH_NOT_FOUND,
                HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        }
    }

    auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
        NULL, NULL, SC_MANAGER_CREATE_SERVICE
    ));

    auto service = UtilGetSharedServiceHandle(CreateServiceW(
        manager.get(),
        MA_SERVICE_NAME,
        MA_SERVICE_DISPLAY_NAME,
        SERVICE_START | DELETE,
        SERVICE_KERNEL_DRIVER,
        SERVICE_SYSTEM_START,
        SERVICE_ERROR_NORMAL,
        // On Windows, std::filesystem::canonical removes the '??' prefix.
        // Add it again, since the service manager needs the prefix to properly find the driver.
        //
        // Contrary to the docs on Microsoft, even for paths with whitespaces, we do NOT need the
        // double quotes.
        std::format(L"\\??\\{}", std::filesystem::canonical(fullPath).wstring())
        .c_str(),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    ));

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
