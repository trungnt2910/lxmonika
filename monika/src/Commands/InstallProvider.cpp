#include "Commands/InstallProvider.h"

#include <vector>

#include <Windows.h>
#include <winsvc.h>

#include "constants.h"
#include "resource.h"
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

    auto lxmonika = UtilGetSharedServiceHandle(OpenServiceW(
        manager.get(), MA_SERVICE_NAME, SERVICE_QUERY_STATUS
    ), false);

    if (!lxmonika)
    {
        throw MonikaException(MA_STRING_EXCEPTION_LXMONIKA_NOT_INSTALLED);
    }

    auto service = UtilGetSharedServiceHandle(CreateServiceW(
        manager.get(),
        _serviceName.empty() ? fullPath.stem().wstring().c_str() : _serviceName.c_str(),
        _serviceDisplayName.empty() ? NULL : _serviceDisplayName.c_str(),
        SERVICE_START | DELETE,
        SERVICE_KERNEL_DRIVER,
        SERVICE_SYSTEM_START,
        SERVICE_ERROR_NORMAL,
        std::format(L"\\??\\{}", std::filesystem::canonical(fullPath).wstring())
            .c_str(),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    ));

    SERVICE_STATUS lxmonikaStatus;
    if (!QueryServiceStatus(lxmonika.get(), &lxmonikaStatus))
    {
        return HRESULT_FROM_WIN32(ERROR_SUCCESS_REBOOT_REQUIRED);
    }

    if (lxmonikaStatus.dwCurrentState != SERVICE_RUNNING)
    {
        return HRESULT_FROM_WIN32(ERROR_SUCCESS_REBOOT_REQUIRED);
    }

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
