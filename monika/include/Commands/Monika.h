#pragma once

#include "Command.h"

#include "Commands/Exec.h"
#include "Commands/Install.h"
#include "Commands/Uninstall.h"

class Monika : public Command<>
{
private:
    const Exec _execCommand;
    const Install _installCommand;
    const Uninstall _uninstallCommand;
    bool _shouldPrintInfo = false;
    const Switch<bool> _infoSwitch;
public:
    Monika();

    virtual int Execute() const override;
};
