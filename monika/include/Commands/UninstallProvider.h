#pragma once

#include <string>

#include "Command.h"
#include "Switch.h"

class UninstallProvider : public Command<std::wstring>
{
private:
    const Switch<std::wstring> _rest;
    std::wstring _serviceName;
public:
    UninstallProvider(const CommandBase* parentCommand = nullptr);

    virtual int Execute() const override;
};
