#include "Commands/Uninstall.h"

#include <fstream>

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
    ),
    _uninstallProviderCommand(this)
{
    AddCommand(_uninstallProviderCommand);
}

int
Uninstall::Execute() const
{
    auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
        NULL, NULL, SC_MANAGER_CREATE_SERVICE
    ));

    //
    // Check for dependent services.
    //

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

    HRESULT hresult = HRESULT_FROM_WIN32(ERROR_SUCCESS);

    //
    // Uninstall driver.
    //

    if (!SvUninstallDriver(manager, MA_SERVICE_NAME))
    {
        Win32Exception::ThrowUnless(ERROR_SUCCESS_REBOOT_REQUIRED);
        hresult = HRESULT_FROM_WIN32(GetLastError());
    }

    //
    // Remove core driver if we own it.
    //

    std::filesystem::path coreDriverInstalledPath =
        std::filesystem::path(UtilGetDriversDirectory()) / MA_CORE_DRIVER_NAME;
    std::ifstream coreDriverCopiedDataStream;
    coreDriverCopiedDataStream.open(
        coreDriverInstalledPath.wstring() + L":" MA_CORE_COPIED_STREAM_NAME L":$DATA",
        std::ios_base::in | std::ios_base::binary
    );

    if (coreDriverCopiedDataStream.is_open() && coreDriverCopiedDataStream.get() == 1)
    {
        // lxcore.sys was copied by us during installation.

        // Close the data stream to prevent DeleteFileW from failing.
        coreDriverCopiedDataStream.close();

        if (!DeleteFileW(coreDriverInstalledPath.wstring().c_str()))
        {
            if (MoveFileExW(
                coreDriverInstalledPath.wstring().c_str(),
                NULL,
                MOVEFILE_DELAY_UNTIL_REBOOT
            ))
            {
                hresult = ERROR_SUCCESS_REBOOT_REQUIRED;
            }

            // Ignore further errors since there's no going back now.
        }
    }

    return hresult;
}
