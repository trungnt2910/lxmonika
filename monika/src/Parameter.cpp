#include "Parameter.h"

#include "util.h"

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
    class NullParameter : public Parameter
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
    };

    return NullParameter();
})();
