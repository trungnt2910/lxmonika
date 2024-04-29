#include "Commands/InstallProvider.h"

#include <vector>

#include <Windows.h>
#include <winsvc.h>

#include "constants.h"
#include "resource.h"
#include "service.h"
#include "util.h"

#include "Exception.h"

InstallProvider::InstallProvider(const CommandBase* parentCommand)
  : Command(
        MA_STRING_INSTALL_PROVIDER_COMMAND_NAME,
        MA_STRING_INSTALL_PROVIDER_COMMAND_DESCRIPTION,
        _rest,
        parentCommand
    ),
    _rest(
        -1, -1, MA_STRING_INSTALL_PROVIDER_COMMAND_ARGUMENT_DESCRIPTION,
        DriverPathParameter, _path, true
    ),
    _serviceNameSwitch(
        MA_STRING_INSTALL_PROVIDER_SWITCH_SERVICE_NAME_NAME, -1,
        MA_STRING_INSTALL_PROVIDER_SWITCH_SERVICE_NAME_DESCRIPTION,
        StringParameter, _serviceName, true
    ),
    _serviceDisplayNameSwitch(
        MA_STRING_INSTALL_PROVIDER_SWITCH_SERVICE_DISPLAY_NAME_NAME, -1,
        MA_STRING_INSTALL_PROVIDER_SWITCH_SERVICE_DISPLAY_NAME_DESCRIPTION,
        StringParameter, _serviceDisplayName, true
    )
{
    AddSwitch(_serviceNameSwitch);
    AddSwitch(_serviceDisplayNameSwitch);
}

int
InstallProvider::Execute() const
{
    if (!_path.has_value())
    {
        throw MonikaException(
            MA_STRING_EXCEPTION_VALUE_EXPECTED,
            ERROR_INVALID_PARAMETER,
            _rest.GetParameterName()
        );
    }

    std::filesystem::path fullPath = std::filesystem::canonical(_path.value());

    auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
        NULL, NULL, SC_MANAGER_CREATE_SERVICE
    ));

    if (!SvIsLxMonikaRunning(manager))
    {
        throw MonikaException(MA_STRING_EXCEPTION_LXMONIKA_NOT_INSTALLED);
    }

    auto service = SvInstallDriver(
        manager,
        SERVICE_START | DELETE,
        _serviceName.empty() ? fullPath.stem().wstring() : _serviceName,
        _serviceDisplayName.empty() ? std::optional<std::wstring>() : _serviceDisplayName,
        std::nullopt,
        fullPath,
        std::vector<std::wstring>{ MA_SERVICE_NAME }
    );

    if (!StartServiceW(service.get(), 0, NULL))
    {
        int code = GetLastError();
        switch (code)
        {
        case ERROR_NOT_FOUND:
        {
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
