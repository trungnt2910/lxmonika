#include "Monika.h"

#include <iostream>

#include "resource.h"
#include "util.h"

Monika::Monika()
    :   Command(
            MA_STRING_MONIKA_COMMAND_NAME,
            MA_STRING_MONIKA_COMMAND_DESCRIPTION,
            NullSwitch
        )
{
    // Currently no-op.
}

int
Monika::Execute() const
{
    std::wcout << UtilGetResourceString(MA_STRING_MONIKA_JUST_MONIKA) << std::endl;
    return 0;
}
