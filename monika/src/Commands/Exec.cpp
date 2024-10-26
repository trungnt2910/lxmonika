#include "Commands/Exec.h"

#include <Windows.h>
#include <winternl.h>

#include <lxmonika/reality.h>

#include "resource.h"
#include "service.h"
#include "util.h"

#include "Exception.h"

Exec::Exec(const CommandBase* parentCommand)
  : Command(
        MA_STRING_EXEC_COMMAND_NAME,
        MA_STRING_EXEC_COMMAND_DESCRIPTION,
        _rest,
        parentCommand
    ),
    _rest(
        -1, -1, MA_STRING_EXEC_COMMAND_ARGUMENT_DESCRIPTION,
        ArgumentsParameter, _arguments, false
    ),
    _providerNameSwitch(
        MA_STRING_EXEC_SWITCH_PROVIDER_NAME_NAME, -1,
        MA_STRING_EXEC_SWITCH_PROVIDER_NAME_DESCRIPTION,
        StringParameter, _providerName, true
    ),
    _providerArgsSwitch(
        MA_STRING_EXEC_SWITCH_PROVIDER_ARGS_NAME, -1,
        MA_STRING_EXEC_SWITCH_PROVIDER_ARGS_DESCRIPTION,
        ArgumentsParameter, _providerArgs, true
    ),
    _rootSwitch(
        MA_STRING_EXEC_SWITCH_ROOT_NAME, -1,
        MA_STRING_EXEC_SWITCH_ROOT_DESCRIPTION,
        PathParameter, _root, true
    ),
    _currentDirectorySwitch(
        MA_STRING_EXEC_SWITCH_CURRENT_DIRECTORY_NAME, -1,
        MA_STRING_EXEC_SWITCH_CURRENT_DIRECTORY_DESCRIPTION,
        PathParameter, _currentDirectory, true
    ),
    _argumentsSwitch(
        MA_STRING_EXEC_SWITCH_ARGUMENTS_NAME, -1,
        MA_STRING_EXEC_SWITCH_ARGUMENTS_DESCRIPTION,
        ArgumentsParameter, _arguments, false
    )
{
    AddSwitch(_providerNameSwitch);
    AddSwitch(_providerArgsSwitch);
    AddSwitch(_rootSwitch);
    AddSwitch(_currentDirectorySwitch);
    AddSwitch(_argumentsSwitch);
}

int
Exec::Execute() const
{
    auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
        NULL, NULL, GENERIC_READ
    ));

    if (!SvIsLxMonikaInstalled(manager))
    {
        throw MonikaException(MA_STRING_EXCEPTION_LXMONIKA_NOT_INSTALLED);
    }

    auto reality = UtilGetSharedWin32Handle(CreateFileW(
        L"\\\\?\\GLOBALROOT" RL_DEVICE_NAME,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    ));

    RL_PICO_SESSION_ATTRIBUTES picoSessionAttributes =
    {
        .Size = sizeof(RL_PICO_SESSION_ATTRIBUTES)
    };

    // Provider Name

    UNICODE_STRING strProviderName;
    if (_providerName.has_value())
    {
        RtlInitUnicodeString(&strProviderName, _providerName.value().c_str());
        picoSessionAttributes.ProviderName = &strProviderName;
    }
    else
    {
        picoSessionAttributes.ProviderIndex = 0;
    }

    // Root Directory

    UNICODE_STRING strRoot;
    std::wstring rootNt = UtilWin32ToNtPath(_root.value_or(std::filesystem::current_path()));
    RtlInitUnicodeString(&strRoot, rootNt.c_str());
    picoSessionAttributes.RootDirectory = &strRoot;

    // Current Directory

    UNICODE_STRING strCurrentDirectory;
    std::wstring currentDirectoryNt =
        UtilWin32ToNtPath(_currentDirectory.value_or(std::filesystem::current_path()));
    RtlInitUnicodeString(&strCurrentDirectory, currentDirectoryNt.c_str());
    picoSessionAttributes.CurrentWorkingDirectory = &strCurrentDirectory;

    // Provider Arguments

    picoSessionAttributes.ProviderArgsCount = _providerArgs.size();
    std::vector<UNICODE_STRING> strProviderArgs(_providerArgs.size());
    for (size_t i = 0; auto& strArg: strProviderArgs)
    {
        RtlInitUnicodeString(&strArg, _providerArgs[i].c_str());
    }
    picoSessionAttributes.ProviderArgs = strProviderArgs.data();

    // Process Arguments

    picoSessionAttributes.ArgsCount = _arguments.size();
    std::vector<UNICODE_STRING> strArgs(_arguments.size());
    for (size_t i = 0; auto& strArg: strArgs)
    {
        RtlInitUnicodeString(&strArg, _arguments[i].c_str());
    }
    picoSessionAttributes.Args = strArgs.data();

    // Environment Variables

    std::vector<std::wstring> environment;
    {
        auto ptrEnvStrings = std::shared_ptr<WCHAR>(
            GetEnvironmentStringsW(), FreeEnvironmentStringsW
        );
        LPWCH lpwEnvStrings = ptrEnvStrings.get();
        while (*lpwEnvStrings != L'\0')
        {
            environment.emplace_back(lpwEnvStrings);
            lpwEnvStrings += environment.back().size() + 1;
        }
    }

    picoSessionAttributes.EnvironmentCount = environment.size();
    std::vector<UNICODE_STRING> strEnvironment(environment.size());
    for (size_t i = 0; auto& strArg: strEnvironment)
    {
        RtlInitUnicodeString(&strArg, environment[i].c_str());
    }
    picoSessionAttributes.Environment = strEnvironment.data();

    // IOCTL to launch the process.

    NTSTATUS statusExecute = 0;
    DWORD dwBytesReturned = 0;

    Win32Exception::ThrowIfFalse(DeviceIoControl(
        reality.get(),
        RL_IOCTL_PICO_START_SESSION,
        &picoSessionAttributes,
        sizeof(picoSessionAttributes),
        &statusExecute,
        sizeof(statusExecute),
        &dwBytesReturned,
        NULL
    ));

    return RtlNtStatusToDosError(statusExecute);
}
