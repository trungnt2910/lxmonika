#include "Commands/UninstallProvider.h"

#include "resource.h"
#include "service.h"
#include "util.h"

#include "Exception.h"

UninstallProvider::UninstallProvider(const CommandBase* parentCommand)
    : Command(
        MA_STRING_UNINSTALL_PROVIDER_COMMAND_NAME,
        MA_STRING_UNINSTALL_PROVIDER_COMMAND_DESCRIPTION,
        _rest,
        parentCommand
    ),
    _rest(
        -1, -1, MA_STRING_UNINSTALL_PROVIDER_COMMAND_ARGUMENT_DESCRIPTION,
        ServiceNameParameter, _serviceName, true
    )
{
    // Currently no-op.
}

int
UninstallProvider::Execute() const
{
    auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
        NULL, NULL, SC_MANAGER_CREATE_SERVICE
    ));

    // TODO: Properly ensure that lxmonika has cleaned up all of this provider's Pico processes.
    // Otherwise, BSODs may occur.
    Win32Exception::ThrowIfFalse(SvUninstallDriver(manager, _serviceName));

    return 0;
}
