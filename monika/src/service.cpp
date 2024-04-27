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
    const std::wstring_view& serviceName,
    const std::optional<std::wstring_view>& displayName,
    const std::optional<std::wstring_view>& description,
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
                SERVICE_SYSTEM_START,
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
