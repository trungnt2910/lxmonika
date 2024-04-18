#pragma once

#include "Command.h"

class Monika : public Command<>
{
public:
    Monika();

    virtual int Execute() const override;
};
