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

    Win32Exception::ThrowIfFalse(SvUninstallDriver(manager, MA_SERVICE_NAME));

    return 0;
}
