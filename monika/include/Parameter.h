#pragma once

#include <any>
#include <string_view>
#include <typeinfo>

class Parameter
{
private:
    const std::wstring_view _name;
protected:
    Parameter(int type);
    Parameter(std::wstring_view name) : _name(name) { }
public:
    virtual std::any Parse(int& argc, wchar_t**& argv,
        const std::type_info& type, bool more) const = 0;

    const std::wstring_view& GetName() const { return _name; }

    bool operator!=(const Parameter& other) const { return this != &other; }
};

extern const Parameter& NullParameter;
extern const Parameter& ArgumentsParameter;
extern const Parameter& DriverPathParameter;
extern const Parameter& PathParameter;
extern const Parameter& ServiceNameParameter;
extern const Parameter& StringParameter;
