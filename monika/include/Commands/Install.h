#pragma once

#include <filesystem>
#include <optional>

#include "Command.h"

class Install : public Command<std::optional<std::filesystem::path>>
{
private:
    std::optional<std::filesystem::path> _path;
public:
    Install(const CommandBase* parentCommand = nullptr);

    virtual int Execute() const override;
};
