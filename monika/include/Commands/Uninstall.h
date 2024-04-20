#pragma once

#include "Command.h"

class Uninstall : public Command<>
{
public:
    Uninstall(const CommandBase* parentCommand = nullptr);

    virtual int Execute() const override;
};
