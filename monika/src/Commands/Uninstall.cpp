#include "Commands/Uninstall.h"

#include "constants.h"
#include "resource.h"
#include "service.h"
#include "util.h"

#include "Exception.h"
#include "Switch.h"

Uninstall::Uninstall(const CommandBase* parentCommand)
    : Command(
        MA_STRING_UNINSTALL_COMMAND_NAME,
        MA_STRING_UNINSTALL_COMMAND_DESCRIPTION,
        NullSwitch,
        parentCommand
    )
{
    // Currently no-op.
}


int
Uninstall::Execute() const
{
    auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
        NULL, NULL, SC_MANAGER_CREATE_SERVICE
    ));

    auto service = UtilGetSharedServiceHandle(OpenServiceW(
        manager.get(), MA_SERVICE_NAME, SERVICE_STOP | DELETE
    ));

    // fail if any service is using lxmonika.
    std::vector<char> buffer;
    if (SvGetLxMonikaDependentServices(
        manager,
        buffer
    ).size() > 0)
    {
        throw MonikaException(
            MA_STRING_EXCEPTION_LXMONIKA_DEPENDENT_STILL_INSTALLED,
            HRESULT_FROM_WIN32(ERROR_BUSY)
        );
    }

    Win32Exception::ThrowIfFalse(DeleteService(service.get()));

    SERVICE_STATUS serviceStatus{ .dwCurrentState = 0 };
    ControlService(service.get(), SERVICE_CONTROL_STOP, &serviceStatus);

    std::filesystem::path installPath = std::filesystem::canonical(
        std::filesystem::path(UtilGetSystemDirectory()) / "drivers" / MA_SERVICE_DRIVER_NAME
    );

    if (serviceStatus.dwCurrentState != SERVICE_STOPPED)
    {
        // delay delete lxmonika.
        Win32Exception::ThrowIfFalse(MoveFileExW(
            installPath.wstring().c_str(),
            NULL,
            MOVEFILE_DELAY_UNTIL_REBOOT
        ));

        return HRESULT_FROM_WIN32(ERROR_SUCCESS_REBOOT_REQUIRED);
    }

    // delete lxmonika
    if (!DeleteFileW(installPath.wstring().c_str()))
    {
        // if fail, delay delete lxmonika.sys.
        Win32Exception::ThrowIfFalse(MoveFileExW(
            installPath.wstring().c_str(),
            NULL,
            MOVEFILE_DELAY_UNTIL_REBOOT
        ));

        return HRESULT_FROM_WIN32(ERROR_SUCCESS_REBOOT_REQUIRED);
    }

    return 0;
}
