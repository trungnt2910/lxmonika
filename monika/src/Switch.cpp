#include "Switch.h"

#include "util.h"

#include "Parameter.h"

extern const Switch<>& NullSwitch = Switch<>(NullParameter,
    const_cast<std::remove_cv_t<decltype(std::ignore)>&>(std::ignore), true);

SwitchBase::SwitchBase(
    int name,
    int $short,
    int description,
    const Parameter& parameter,
    const std::type_info& parameterType,
    void* parameterOutput,
    bool acceptsMoreSwitches
) : _name(UtilGetResourceString(name)),
    _short($short >= 0 ? std::optional(UtilGetResourceString($short)) : std::nullopt),
    _description(UtilGetResourceString(description)),
    _parameter(parameter),
    _parameterType(parameterType),
    _parameterOutput(parameterOutput),
    _acceptsMoreSwitches(acceptsMoreSwitches)
{
    // Currently no-op.
}

SwitchBase::SwitchBase(
    const Parameter& parameter,
    const std::type_info& parameterType,
    void* parameterOutput,
    bool acceptsMoreSwitches
) : _parameter(parameter),
    _parameterType(parameterType),
    _parameterOutput(parameterOutput),
    _acceptsMoreSwitches(acceptsMoreSwitches)
{
    // Currently no-op.
}
