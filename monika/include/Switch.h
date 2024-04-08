#pragma once

#include <optional>
#include <string_view>
#include <typeinfo>
#include <type_traits>

#include "Parameter.h"

class SwitchBase
{
    template <typename T>
    friend class Switch;

private:
    const std::wstring_view _name;
    const std::optional<std::wstring_view> _short;
    const std::wstring_view _description;
    const Parameter& _parameter;
    const std::type_info& _parameterType;
    void const* _parameterOutput;
    const bool _acceptsMoreSwitches;
protected:
    SwitchBase(int name, int $short, int description,
        const Parameter& parameter,
        const std::type_info& parameterType,
        void* parameterOutput,
        bool acceptsMoreSwitches);
    SwitchBase(
        const Parameter& parameter,
        const std::type_info& parameterType,
        void* parameterOutput,
        bool acceptsMoreSwitches);
public:
    virtual bool Parse(int& argc, wchar_t**& argv) const = 0;

    const Parameter& GetParameter() const { return _parameter; }
    const std::wstring_view& GetParameterName() const { return _parameter.GetName(); }
    const std::wstring_view& GetName() const { return _name; }
    const std::optional<std::wstring_view>& GetShort() const { return _short; }
    const std::wstring_view& GetDescription() const { return _description; }
};

template <typename T = std::remove_cv_t<decltype(std::ignore)>>
class Switch : public SwitchBase
{
protected:
    virtual void Validate(const T& data) const { }

    virtual bool Parse(int& argc, wchar_t**& argv) const
    {
        T& parameterOutputReference = *((T*)_parameterOutput);

        parameterOutputReference = std::any_cast<T>(
            _parameter.Parse(argc, argv, _parameterType)
        );

        Validate(parameterOutputReference);

        return _acceptsMoreSwitches;
    }
public:
    Switch(int name, int $short, int description,
        const Parameter& parameter,
        T& parameterOutput,
        bool acceptsMoreSwitches)
        : SwitchBase(name, $short, description,
            parameter, typeid(T), &parameterOutput, acceptsMoreSwitches) { }
    Switch(
        const Parameter& parameter,
        T& parameterOutput,
        bool acceptsMoreSwitches)
        : SwitchBase(parameter, typeid(T), &parameterOutput, acceptsMoreSwitches) { }
};

extern const Switch<>& NullSwitch;
