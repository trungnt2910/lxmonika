#include "Parameter.h"

#include <filesystem>
#include <fstream>

#include "resource.h"
#include "service.h"
#include "util.h"

#include "Exception.h"

Parameter::Parameter(
    int type
) : Parameter(UtilGetResourceString(type))
{
    // Currently no-op.
}

//
// AbstractStringParameter
//

// While this class works exactly the same way as normal StringParameter,
// we also want others to inherit it as well. If we define StringParameter here,
// when other classes in this file inherit "StringParameter", MSVC will produce an
// internal compiler error, possibly due to having the same name as the exported
// constant, "extern const Parameter& StringParameter;".
// Therefore, we create an "AbstractStringParameter" here, then define our
// invisible StringParameter class later.

class AbstractStringParameter : public Parameter
{
public:
    virtual std::any Parse(int& argc, wchar_t**& argv,
        const std::type_info& type) const override final
    {
        std::optional<std::wstring_view> arg;

        if (argc > 0)
        {
            --argc;
            arg = std::wstring_view(*argv);
            try
            {
                if (!Validate(arg.value()))
                {
                    throw nullptr;
                }
            }
            catch (Exception&)
            {
                throw;
            }
            catch (...)
            {
                throw Win32Exception(ERROR_INVALID_PARAMETER);
            }
            ++argv;
        }

        return Convert(arg, type);
    }
protected:
    AbstractStringParameter(int type) : Parameter(type) { }
    virtual bool Validate(const std::wstring_view& arg) const
    {
        return true;
    }
    const std::wstring_view& EnsureHasValue(
        const std::optional<std::wstring_view>& arg) const
    {
        if (!arg.has_value())
        {
            throw MonikaException(
                MA_STRING_EXCEPTION_VALUE_EXPECTED,
                HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
                GetName()
            );
        }
        return arg.value();
    }
    virtual std::any Convert(const std::optional<std::wstring_view>& arg,
        const std::type_info& type) const
    {
        if (type == typeid(std::string))
        {
            const std::wstring_view& value = EnsureHasValue(arg);
            std::string result;
            result.resize(value.size());
            std::transform(value.begin(), value.end(), result.begin(),
                [](wchar_t wc) { return (char)wc; });
            return result;
        }
        else if (type == typeid(std::wstring))
        {
            const std::wstring_view& value = EnsureHasValue(arg);
            return std::wstring(value);
        }
        else if (type == typeid(std::wstring_view))
        {
            const std::wstring_view& value = EnsureHasValue(arg);
            return value;
        }
        else if (type == typeid(std::optional<std::string>))
        {
            if (arg.has_value())
            {
                std::string result;
                result.resize(arg.value().size());
                std::transform(arg.value().begin(), arg.value().end(), result.begin(),
                    [](wchar_t wc) { return (char)wc; });
                return std::optional(result);
            }
            else
            {
                return (std::optional<std::wstring>)std::nullopt;
            }
        }
        else if (type == typeid(std::optional<std::wstring>))
        {
            return arg.has_value() ?
                std::optional(std::wstring(arg.value())) : std::nullopt;
        }
        else if (type == typeid(std::optional<std::wstring_view>))
        {
            return arg.has_value() ?
                std::optional(arg) : std::nullopt;
        }
        return nullptr;
    }
};

//
// NullParameter
//

extern const Parameter& NullParameter = ([]()
{
    static const class NullParameter : public Parameter
    {
    public:
        NullParameter() : Parameter(L"") { }
        virtual std::any Parse(int& argc, wchar_t**& argv,
            const std::type_info& type) const
        {
            if (type == typeid(std::ignore))
            {
                return std::ignore;
            }
            else if (type == typeid(bool))
            {
                return true;
            }
            else if (type == typeid(std::wstring))
            {
                return std::wstring();
            }
            else if (type == typeid(std::wstring_view))
            {
                return std::wstring_view();
            }

            return nullptr;
        }
    } NullParameter;

    return NullParameter;
})();

//
// StringParameter
//

extern const Parameter& StringParameter = ([]()
{
    static const class StringParameter : public AbstractStringParameter
    {
    public:
        StringParameter() : AbstractStringParameter(MA_STRING_PARAMETER_NAME_STRING) { }
    } StringParameter;

    return StringParameter;
})();

//
// DriverPathParameter
//

extern const Parameter& DriverPathParameter = ([]()
{
    static const class DriverPathParameter : public AbstractStringParameter
    {
    public:
        DriverPathParameter() : AbstractStringParameter(MA_STRING_PARAMETER_NAME_DRIVER_PATH) { }
    protected:
        virtual bool Validate(const std::wstring_view& arg) const override
        {
            try
            {
                // Check extension.
                if (std::filesystem::path(arg).extension().wstring() != L".sys")
                {
                    throw nullptr;
                }

                // Check exists and PE magic.
                std::ifstream fin;
                fin.open(arg, std::ios_base::in | std::ios_base::binary);
                char buffer[2] = { };
                fin.read(buffer, 2);
                if (buffer[0] != 'M' || buffer[1] != 'Z')
                {
                    throw nullptr;
                }
            }
            catch (...)
            {
                throw MonikaException(
                    MA_STRING_EXCEPTION_INVALID_DRIVER_PATH,
                    HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
                    arg
                );
            }

            return AbstractStringParameter::Validate(arg);
        }
        virtual std::any Convert(const std::optional<std::wstring_view>& arg,
            const std::type_info& type) const override
        {
            if (type == typeid(std::filesystem::path))
            {
                const std::wstring_view& value = EnsureHasValue(arg);
                return std::filesystem::path(value);
            }
            else if (type == typeid(std::optional<std::filesystem::path>))
            {
                return arg.has_value() ?
                    std::optional(std::filesystem::path(arg.value())) : std::nullopt;
            }
            else
            {
                return AbstractStringParameter::Convert(arg, type);
            }
        }
    } DriverPathParameter;

    return DriverPathParameter;
})();

//
// ServiceNameParameter
//

extern const Parameter& ServiceNameParameter = ([]()
{
    static const class ServiceNameParameter : public AbstractStringParameter
    {
    public:
        ServiceNameParameter() : AbstractStringParameter(MA_STRING_PARAMETER_NAME_SERVICE_NAME) { }
    protected:
        virtual bool Validate(const std::wstring_view& arg) const override
        {
            try
            {
                auto manager = UtilGetSharedServiceHandle(OpenSCManagerW(
                    NULL, NULL, SC_MANAGER_CREATE_SERVICE | SC_MANAGER_ENUMERATE_SERVICE
                ));

                std::vector<char> buffer;
                std::span<const ENUM_SERVICE_STATUSW> spanEnumSerivceStatus =
                    SvGetLxMonikaDependentServices(manager, buffer);

                if (std::none_of(
                    spanEnumSerivceStatus.begin(),
                    spanEnumSerivceStatus.end(),
                    [&](const ENUM_SERVICE_STATUSW& status)
                    {
                        return _wcsicmp(status.lpDisplayName, arg.data()) == 0;
                    }
                ))
                {
                    throw nullptr;
                }

                return true;
            }
            catch (Exception&)
            {
                // Another component is throwing an exception.
                throw;
            }
            catch (...)
            {
                // We got some generic exception that we don't know how to handle,
                // or we have a nullptr here due to the check failed.
                // Fail it as a generic error for this category.
                throw MonikaException(
                    MA_STRING_EXCEPTION_UNKNOWN_SERVICE,
                    HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
                    arg
                );
            }

            return AbstractStringParameter::Validate(arg);
        }
    } ServiceNameParameter;

    return ServiceNameParameter;
})();
