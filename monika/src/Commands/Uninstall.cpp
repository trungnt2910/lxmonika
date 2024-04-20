#include "Commands/Uninstall.h"

#include "constants.h"
#include "resource.h"
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

    if (!DeleteService(service.get()))
    {
        throw Win32Exception();
    }

    SERVICE_STATUS serviceStatus{ .dwCurrentState = 0 };
    ControlService(service.get(), SERVICE_CONTROL_STOP, &serviceStatus);

    if (serviceStatus.dwCurrentState != SERVICE_STOPPED)
    {
        return HRESULT_FROM_WIN32(ERROR_SUCCESS_REBOOT_REQUIRED);
    }

    return 0;
}
