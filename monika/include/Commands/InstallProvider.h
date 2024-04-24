#pragma once

#include <filesystem>

#include "Command.h"
#include "Switch.h"

class InstallProvider : public Command<std::optional<std::filesystem::path>>
{
private:
    const Switch<std::optional<std::filesystem::path>> _rest;
    const Switch<std::wstring> _serviceNameSwitch;
    const Switch<std::wstring> _serviceDisplayNameSwitch;
    std::optional<std::filesystem::path> _path;
    std::wstring _serviceName;
    std::wstring _serviceDisplayName;
public:
    InstallProvider(const CommandBase* parentCommand = nullptr);

    virtual int Execute() const override;
};
