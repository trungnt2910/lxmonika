#include "Commands/Monika.h"

#include <iostream>

#include "resource.h"
#include "util.h"

Monika::Monika()
  : Command(
        MA_STRING_MONIKA_COMMAND_NAME,
        MA_STRING_MONIKA_COMMAND_DESCRIPTION,
        NullSwitch
    ),
    _installCommand(this),
    _uninstallCommand(this)
{
    AddCommand(_installCommand);
    AddCommand(_uninstallCommand);
}

int
Monika::Execute() const
{
    std::wcout << UtilGetResourceString(MA_STRING_MONIKA_JUST_MONIKA) << std::endl;
    return 0;
}
