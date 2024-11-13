#pragma once

#include <filesystem>
#include <optional>

#include "Command.h"
#include "Switch.h"

#include "Commands/InstallProvider.h"

class Install : public Command<std::optional<std::filesystem::path>>
{
private:
    const InstallProvider _installProviderCommand;
    const Switch<std::optional<std::filesystem::path>> _rest;
    const Switch<std::optional<std::filesystem::path>> _coreSwitch;
    const Switch<std::optional<std::wstring>> _borrowSwitch;
    const Switch<bool> _enableLateRegistrationSwitch;
    const Switch<bool> _forceSwitch;
    std::optional<std::filesystem::path> _path;
    std::optional<std::filesystem::path> _core;
    std::optional<std::wstring> _borrow;
    bool _enableLateRegistration = false;
    bool _force = false;
public:
    Install(const CommandBase* parentCommand = nullptr);

    virtual int Execute() const override;
};
