#pragma once

#include "Command.h"

#include "Commands/UninstallProvider.h"

class Uninstall : public Command<>
{
private:
    const UninstallProvider _uninstallProviderCommand;
public:
    Uninstall(const CommandBase* parentCommand = nullptr);

    virtual int Execute() const override;
};
