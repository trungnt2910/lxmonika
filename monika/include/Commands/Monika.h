#pragma once

#include "Command.h"

#include "Commands/Install.h"

class Monika : public Command<>
{
private:
    const Install _installCommand;
public:
    Monika();

    virtual int Execute() const override;
};
