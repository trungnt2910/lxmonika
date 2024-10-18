#include "service.h"

#include <filesystem>
#include <functional>

#include <Windows.h>
#include <winsvc.h>
#include <winternl.h>

#include "constants.h"
#include "util.h"

#include "Exception.h"

class Transaction
{
private:
    bool _commit = false;
    const std::function<void()>& _cleanup;
public:
    Transaction(
        const std::function<void()>& action,
        const std::function<void()>& cleanup
    ) : _cleanup(cleanup)
    {
        action();
    }

    Transaction(const Transaction&) = delete;

    void Commit()
    {
        _commit = true;
    }

    ~Transaction()
    {
        if (!_commit)
        {
            _cleanup();
        }
    }
};

ServiceHandle
SvInstallDriver(
    ServiceHandle manager,
    DWORD dwDesiredAccess,
    DWORD dwStartType,
    const std::wstring& serviceName,
    const std::optional<std::wstring>& displayName,
    const std::optional<std::wstring>& description,
    const std::filesystem::path& binaryPath,
    const std::optional<std::vector<std::wstring>>& dependencies
)
{
    std::filesystem::path newPath;
    ServiceHandle service;

    Transaction copyDriverFile(
        [&]()
        {
            std::filesystem::path installDir = UtilGetSystemDirectory();
            installDir /= "drivers";

            std::filesystem::path fullPath = std::filesystem::canonical(binaryPath);
            newPath = installDir / fullPath.filename();

            // Use Win32 copy to get better error messages.
            Win32Exception::ThrowIfFalse(CopyFileW(
                fullPath.wstring().c_str(), newPath.wstring().c_str(), true
            ));
        },
        [&]()
        {
            std::filesystem::remove(newPath);
        }
    );

    Transaction installService(
        [&]()
        {
            const auto GetDependenciesString = [&]()
            {
                if (!dependencies.has_value())
                {
                    return std::wstring();
                }
                std::wstring result;
                for (const auto& value : dependencies.value())
                {
                    result += value;
                    result += L'\0';
                }
                return result;
            };

            service = UtilGetSharedServiceHandle(CreateServiceW(
                manager.get(),
                serviceName.data(),
                displayName.has_value() ? displayName.value().data() : NULL,
                dwDesiredAccess | DELETE |
                    (description.has_value() ? SERVICE_CHANGE_CONFIG : 0),
                SERVICE_KERNEL_DRIVER,
                dwStartType,
                SERVICE_ERROR_NORMAL,
                // On Windows, std::filesystem::canonical removes the '??' prefix.
                // Add it again, since the service manager needs the prefix to properly
                // find the driver.
                //
                // Contrary to the docs on Microsoft, even for paths with whitespaces,
                // we do NOT need the double quotes.
                std::format(L"\\??\\{}", std::filesystem::canonical(newPath).wstring()).c_str(),
                NULL,
                NULL,
                GetDependenciesString().c_str(),
                NULL,
                NULL
            ));
        },
        [&]()
        {
            DeleteService(service.get());
        }
    );

    if (description.has_value())
    {
        SERVICE_DESCRIPTIONW serviceDescription =
        {
            .lpDescription = (LPWSTR)description.value().data()
        };
        Win32Exception::ThrowIfFalse(ChangeServiceConfig2W(
            service.get(), SERVICE_CONFIG_DESCRIPTION, &serviceDescription
        ));
    }

    copyDriverFile.Commit();
    installService.Commit();

    return service;
}

bool
SvUninstallDriver(
    ServiceHandle manager,
    const std::wstring& serviceName
)
{
    auto service = UtilGetSharedServiceHandle(OpenServiceW(
        manager.get(), serviceName.c_str(), SERVICE_QUERY_CONFIG | SERVICE_STOP | DELETE
    ));

    std::filesystem::path installPath;
    {
        std::vector<char> buffer;
        LPQUERY_SERVICE_CONFIG pServiceConfig = SvQueryServiceConfig(service, buffer);
        installPath = std::filesystem::canonical(pServiceConfig->lpBinaryPathName);
    }

    Win32Exception::ThrowIfFalse(DeleteService(service.get()));

    SERVICE_STATUS serviceStatus{ .dwCurrentState = 0 };
    ControlService(service.get(), SERVICE_CONTROL_STOP, &serviceStatus);

    if (serviceStatus.dwCurrentState == SERVICE_STOPPED)
    {
        // The service has stopped, we can delete the driver now.
        if (DeleteFileW(installPath.wstring().c_str()))
        {
            // Everything going on well.
            return true;
        }
    }

    // Otherwise, we use this API to try deleting the driver on reboot.
    Win32Exception::ThrowIfFalse(MoveFileExW(
        installPath.wstring().c_str(),
        NULL,
        MOVEFILE_DELAY_UNTIL_REBOOT
    ));

    SetLastError(ERROR_SUCCESS_REBOOT_REQUIRED);
    return false;
}

bool
SvIsLxMonikaInstalled(
    ServiceHandle manager
)
{
    // Basic checks only.
    // For a comprehensive check to see whether lxmonika is actually usable, use
    // SvIsLxMonikaRunning.

    auto lxmonika = UtilGetSharedServiceHandle(OpenServiceW(
        manager.get(), MA_SERVICE_NAME, SERVICE_QUERY_STATUS
    ), false);

    if (lxmonika.get() != NULL)
    {
        return true;
    }

    DWORD code = GetLastError();
    if (code == ERROR_SERVICE_DOES_NOT_EXIST)
    {
        return false;
    }

    throw Win32Exception(code);
}

bool
SvIsLxMonikaRunning(
    ServiceHandle manager
)
{
    std::vector<char> buffer;

    // 5 conditions:
    // - lxmonika service installed.
    // - lxmonika service or any dependent is running.
    // - lxmonika.sys driver file exists in C:\Windows\System32\drivers.
    // - lxmonika binary path correctly set to C:\Windows\System32\drivers\lxmonika.sys.
    // - \Device\Reality exists.

    // lxmonika service installed

    auto lxmonika = UtilGetSharedServiceHandle(OpenServiceW(
        manager.get(),
        MA_SERVICE_NAME,
        SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG
    ), false);

    if (!Win32Exception::ThrowUnless(ERROR_SERVICE_DOES_NOT_EXIST))
    {
        return false;
    }

    // lxmonika service or any dependent is running.

    SERVICE_STATUS serviceStatus;
    Win32Exception::ThrowIfFalse(QueryServiceStatus(lxmonika.get(), &serviceStatus));

    if (serviceStatus.dwServiceType != SERVICE_KERNEL_DRIVER)
    {
        return false;
    }

    if (serviceStatus.dwCurrentState != SERVICE_RUNNING)
    {
        std::vector<char> buffer;
        std::span<const ENUM_SERVICE_STATUSW> spanEnumSerivceStatus =
            SvGetLxMonikaDependentServices(
                manager,
                buffer
            );

        if (std::none_of(
            spanEnumSerivceStatus.begin(),
            spanEnumSerivceStatus.end(),
            [](const ENUM_SERVICE_STATUSW& status)
            {
                return status.ServiceStatus.dwCurrentState == SERVICE_RUNNING;
            }
        ))
        {
            return false;
        }
    }

    // lxmonika.sys driver file exists in C:\Windows\System32.

    std::filesystem::path installPath = UtilGetSystemDirectory();
    installPath /= "drivers";
    installPath /= MA_SERVICE_DRIVER_NAME;

    if (!std::filesystem::exists(installPath))
    {
        return false;
    }

    // - lxmonika binary path correctly set to C:\Windows\System32\drivers\lxmonika.sys.

    LPQUERY_SERVICE_CONFIGW pQueryServiceConfig = SvQueryServiceConfig(lxmonika, buffer);

    if (std::filesystem::canonical(installPath)
        != std::filesystem::weakly_canonical(pQueryServiceConfig->lpBinaryPathName))
    {
        return false;
    }

    // \Device\Reality exists.

    UNICODE_STRING strDevicePath;
    RtlInitUnicodeString(&strDevicePath, L"\\Device\\Reality");

    OBJECT_ATTRIBUTES objAttributes;
    InitializeObjectAttributes(
        &objAttributes,
        &strDevicePath,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
    );

    IO_STATUS_BLOCK ioStatus;

    HANDLE hdlDevice = NULL;
    NTSTATUS status = NtCreateFile(
        &hdlDevice,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        &objAttributes,
        &ioStatus,
        NULL,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_CREATE,
        FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
    );

    if (!NT_SUCCESS(status))
    {
        return false;
    }

    return true;
}

std::span<const ENUM_SERVICE_STATUSW>
SvGetLxMonikaDependentServices(
    ServiceHandle manager,
    std::vector<char>& buffer
)
{
    auto lxmonika = UtilGetSharedServiceHandle(OpenServiceW(
        manager.get(),
        MA_SERVICE_NAME,
        SERVICE_ENUMERATE_DEPENDENTS
    ));

    DWORD cbBytesNeeded;
    DWORD dwServicesReturned;
    while (true)
    {
        if (EnumDependentServicesW(
            lxmonika.get(),
            SERVICE_ACTIVE,
            (LPENUM_SERVICE_STATUSW)buffer.data(),
            (DWORD)buffer.size(),
            &cbBytesNeeded,
            &dwServicesReturned
        ))
        {
            break;
        }

        Win32Exception::ThrowUnless(ERROR_MORE_DATA);
        buffer.resize(cbBytesNeeded);
    }

    return std::span<const ENUM_SERVICE_STATUSW>(
        (LPENUM_SERVICE_STATUSW)buffer.data(),
        dwServicesReturned
    );
}

const LPQUERY_SERVICE_CONFIG
SvQueryServiceConfig(
    ServiceHandle service,
    std::vector<char>& buffer
)
{
    DWORD cbBytesNeeded;

    while (true)
    {
        if (QueryServiceConfigW(
            service.get(),
            (LPQUERY_SERVICE_CONFIGW)buffer.data(),
            (DWORD)buffer.size(),
            &cbBytesNeeded
        ))
        {
            break;
        }

        Win32Exception::ThrowUnless(ERROR_INSUFFICIENT_BUFFER);
        buffer.resize(cbBytesNeeded);
    }

    return (LPQUERY_SERVICE_CONFIGW)buffer.data();
}
