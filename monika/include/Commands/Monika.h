#pragma once

#include "Command.h"

#include "Commands/Install.h"
#include "Commands/Uninstall.h"

class Monika : public Command<>
{
private:
    const Install _installCommand;
    const Uninstall _uninstallCommand;
public:
    Monika();

    virtual int Execute() const override;
};
