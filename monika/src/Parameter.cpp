#include "Parameter.h"

#include <filesystem>
#include <fstream>

#include "resource.h"
#include "util.h"

#include "Exception.h"

Parameter::Parameter(
    int type
) : Parameter(UtilGetResourceString(type))
{
    // Currently no-op.
}

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
                return std::wstring(L"");
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
// DriverPathParameter
//

extern const Parameter& DriverPathParameter = ([]()
{
    static const class DriverPathParameter : public Parameter
    {
    public:
        DriverPathParameter() : Parameter(MA_STRING_PARAMETER_NAME_DRIVER_PATH) { }
        virtual std::any Parse(int& argc, wchar_t**& argv,
            const std::type_info& type) const
        {
            std::wstring_view arg;
            bool hasValue = false;

            if (argc > 0)
            {
                --argc;
                arg = std::wstring_view(*argv);
                ++argv;
                hasValue = true;

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
            }

            const auto ThrowIfNoValue = [&]()
            {
                if (!hasValue)
                {
                    throw MonikaException(
                        MA_STRING_EXCEPTION_VALUE_EXPECTED,
                        HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
                        GetName()
                    );
                }
            };

            if (type == typeid(std::filesystem::path))
            {
                ThrowIfNoValue();
                return std::filesystem::path(arg);
            }
            else if (type == typeid(std::wstring))
            {
                ThrowIfNoValue();
                return std::wstring(arg);
            }
            else if (type == typeid(std::wstring_view))
            {
                ThrowIfNoValue();
                return arg;
            }
            else if (type == typeid(std::optional<std::filesystem::path>))
            {
                return hasValue ? std::optional(std::filesystem::path(arg)) : std::nullopt;
            }
            else if (type == typeid(std::optional<std::wstring>))
            {
                return hasValue ? std::optional(std::wstring(arg)) : std::nullopt;
            }
            else if (type == typeid(std::optional<std::wstring_view>))
            {
                return hasValue ? std::optional(arg) : std::nullopt;
            }

            return nullptr;
        }
    } DriverPathParameter;

    return DriverPathParameter;
})();

//
// StringParameter
//

extern const Parameter& StringParameter = ([]()
{
    static const class StringParameter : public Parameter
    {
    public:
        StringParameter() : Parameter(MA_STRING_PARAMETER_NAME_STRING) { }
        virtual std::any Parse(int& argc, wchar_t**& argv,
            const std::type_info& type) const
        {
            std::wstring_view arg;
            bool hasValue = false;

            if (argc > 0)
            {
                --argc;
                arg = std::wstring_view(*argv);
                ++argv;
                hasValue = true;
            }

            const auto ThrowIfNoValue = [&]()
            {
                if (!hasValue)
                {
                    throw MonikaException(
                        MA_STRING_EXCEPTION_VALUE_EXPECTED,
                        HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
                        GetName()
                    );
                }
            };

            if (type == typeid(std::string))
            {
                ThrowIfNoValue();
                std::string result;
                result.resize(arg.size());
                std::transform(arg.begin(), arg.end(), result.begin(),
                    [](wchar_t wc) { return (char)wc; });
                return result;
            }
            else if (type == typeid(std::wstring))
            {
                ThrowIfNoValue();
                return std::wstring(arg);
            }
            else if (type == typeid(std::wstring_view))
            {
                ThrowIfNoValue();
                return arg;
            }
            else if (type == typeid(std::optional<std::string>))
            {
                if (hasValue)
                {
                    std::string result;
                    result.resize(arg.size());
                    std::transform(arg.begin(), arg.end(), result.begin(),
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
                return hasValue ? std::optional(std::wstring(arg)) : std::nullopt;
            }
            else if (type == typeid(std::optional<std::wstring_view>))
            {
                return hasValue ? std::optional(arg) : std::nullopt;
            }

            return nullptr;
        }
    } StringParameter;

    return StringParameter;
})();
