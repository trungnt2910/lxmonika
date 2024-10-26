#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Command.h"
#include "Switch.h"

class Exec : public Command<std::vector<std::wstring>>
{
private:
    const Switch<std::vector<std::wstring>> _rest;
    const Switch<std::optional<std::wstring>> _providerNameSwitch;
    const Switch<std::vector<std::wstring>> _providerArgsSwitch;
    const Switch<std::optional<std::filesystem::path>> _rootSwitch;
    const Switch<std::optional<std::filesystem::path>> _currentDirectorySwitch;
    const Switch<std::vector<std::wstring>> _argumentsSwitch;
    std::vector<std::wstring> _arguments;
    std::optional<std::wstring> _providerName;
    std::vector<std::wstring> _providerArgs;
    std::optional<std::filesystem::path> _root;
    std::optional<std::filesystem::path> _currentDirectory;
public:
    Exec(const CommandBase* parentCommand = nullptr);

    virtual int Execute() const override;
};
