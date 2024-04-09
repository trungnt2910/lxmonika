#include "Command.h"

#include <algorithm>
#include <format>
#include <iomanip>
#include <iostream>
#include <vector>

#include "resource.h"
#include "util.h"

#include "Exception.h"

//
// Helper macros
//

#ifndef MONIKA_BUILD_AUTHOR
#define MONIKA_BUILD_AUTHOR "lxmonika Authors & Contributors"
#endif

#ifndef MONIKA_BUILD_YEAR
#define MONIKA_BUILD_YEAR   (&__DATE__[7])
#endif

#define WIDEN(x)            WIDEN2(x)
#define WIDEN2(x)           L##x

//
// CommandBase
//

CommandBase::CommandBase(
    int name,
    int description,
    const SwitchBase& rest,
    const CommandBase* parentCommand
) : _name(UtilGetResourceString(name)),
    _description(UtilGetResourceString(description)),
    _rest(rest),
    _parentCommand(parentCommand),
    _helpSwitch(
        MA_STRING_COMMAND_SWITCH_HELP_NAME,
        -1,
        MA_STRING_COMMAND_SWITCH_HELP_DESCRIPTION,
        NullParameter,
        _shouldPrintHelp,
        false
    )
{
    AddSwitch(_helpSwitch);
}

void
CommandBase::AddSwitch(
    const SwitchBase& $switch
)
{
    if (_switches.contains($switch.GetName())
        || ($switch.GetShort().has_value() && _switches.contains($switch.GetShort().value())))
    {
        throw MonikaException(MA_STRING_EXCEPTION_COMMAND_NAME_CONFLICT);
    }

    _switches[$switch.GetName()] = &$switch;
    if ($switch.GetShort().has_value())
    {
        _switches[$switch.GetShort().value()] = &$switch;
    }

    _switchList.push_back(&$switch);
}

void
CommandBase::AddCommand(
    const CommandBase& command
)
{
    if (_subcommands.contains(command.GetName()))
    {
        throw MonikaException(MA_STRING_EXCEPTION_COMMAND_NAME_CONFLICT);
    }

    _subcommands[command.GetName()] = &command;
    _subcommandList.push_back(&command);
}

int
CommandBase::Run(
    int& argc,
    wchar_t**& argv
) const
{
    // The caller is responsible for incrementing argv and decrementing argv.
    // The passed parameters should include only the arguments and not the name itself.

    int originalArgc = argc;

    if (argc > 0 && _subcommands.contains(argv[0]))
    {
        return _subcommands.at(argv[0])->Run(--argc, ++argv);
    }

    // Now get the switches.

    bool acceptsMoreSwitches = true;

    while (acceptsMoreSwitches
        && argc > 0
        && *argv[0] == L'-'
        && _switches.contains(argv[0]))
    {
        acceptsMoreSwitches = _switches.at(argv[0])->Parse(--argc, ++argv);
    }

    // Get the rest of the input.

    if (acceptsMoreSwitches)
    {
        acceptsMoreSwitches = _rest.Parse(argc, argv);
    }

    // Maybe some more switches after the arguments?

    while (acceptsMoreSwitches
        && argc > 0
        && *argv[0] == L'-'
        && _switches.contains(argv[0]))
    {
        acceptsMoreSwitches = _switches.at(argv[0])->Parse(--argc, ++argv);
    }

    // Invalid subcommand.

    if (argc == originalArgc
        && argc > 0
        && *argv[0] != L'-')
    {
        std::wcerr << std::vformat(
            UtilGetResourceString(MA_STRING_COMMAND_INVALID_SUBCOMMAND),
            std::make_wformat_args(argv[0])
        ) << std::endl;
        return HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
    }

    if (acceptsMoreSwitches
        && argc > 0)
    {
        if (*argv[0] == L'-')
        {
            // Invalid option.
            std::wcerr << std::vformat(
                UtilGetResourceString(MA_STRING_COMMAND_INVALID_OPTION),
                std::make_wformat_args(argv[0])
            ) << std::endl;
            return HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
        }
        else
        {
            // Invalid argument.
            std::wcerr << std::vformat(
                UtilGetResourceString(MA_STRING_COMMAND_INVALID_ARGUMENT),
                std::make_wformat_args(argv[0])
            ) << std::endl;
            return HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
        }
    }
    try
    {
        if (_shouldPrintHelp)
        {
            PrintHelp(std::wcout);
            return 0;
        }

        int result = Execute();
        std::wcerr << UtilGetErrorMessage(result) << std::endl;
        return result;
    }
    catch (Exception& exception)
    {
        std::wcerr << exception.Message() << std::endl;
        return exception.GetHresult();
    }
    catch (std::exception& exception)
    {
        std::wcerr << exception.what() << std::endl;
        return HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR);
    }
}

void
CommandBase::PrintHelp(
    std::wostream& os
) const
{
    // Description and optional copyright
    os << _description << L"\n";

    if (_parentCommand == nullptr)
    {
        os << std::vformat(
            UtilGetResourceString(MA_STRING_COMMAND_HELP_COPYRIGHT),
            std::make_wformat_args(
                L"2024",
                strcmp(MONIKA_BUILD_YEAR, "2024") == 0 ? L"" : L"-" WIDEN(MONIKA_BUILD_YEAR),
                WIDEN(MONIKA_BUILD_AUTHOR)
            )
        ) << L"\n";
    }

    // Usage
    std::vector<std::wstring_view> commandTree;
    const CommandBase* current = this;

    while (current != nullptr)
    {
        commandTree.emplace_back(current->_name);
        current = current->_parentCommand;
    }

    os << L"\n";
    os << UtilGetResourceString(MA_STRING_COMMAND_HELP_USAGE) << ":";
    while (!commandTree.empty())
    {
        os << " " << commandTree.back();
        commandTree.pop_back();
    }
    if (!_subcommands.empty())
    {
        os << " [" << UtilGetResourceString(MA_STRING_COMMAND_HELP_PLACEHOLDER_COMMAND) << "]";
    }
    if (!_switches.empty())
    {
        os << " [" << UtilGetResourceString(MA_STRING_COMMAND_HELP_PLACEHOLDER_OPTIONS) << "]";
    }
    if (_rest.GetParameter() != NullParameter)
    {
        // Has arguments
        os << " <" << _rest.GetParameterName() << ">";
    }
    os << L"\n";

    const auto PrintTwoColumns = [&](
        const std::vector<std::wstring>& v1,
        const std::vector<std::wstring>& v2,
        const std::wstring& indent = L""
    )
    {
        size_t len =
            std::max_element(v1.begin(), v1.end(),
                [](auto e1, auto e2) { return e1.size() < e2.size(); })
            ->size();

        len += 2;

        for (size_t i = 0; i < v1.size(); ++i)
        {
            os << indent << std::setw(len) << std::left << v1[i] << v2[i] << L"\n";
        }
    };

    // Arguments
    if (_rest.GetParameter() != NullParameter)
    {
        os << L"\n";
        os << UtilGetResourceString(MA_STRING_COMMAND_HELP_ARGUMENTS) << L":\n";

        PrintTwoColumns(
            { std::format(L"<{}>", _rest.GetParameterName()) },
            { std::wstring(_rest.GetDescription()) },
            std::wstring(2, L' ')
        );
    }

    // Options
    if (!_switches.empty())
    {
        os << L"\n";
        os << UtilGetResourceString(MA_STRING_COMMAND_HELP_OPTIONS) << L":\n";

        std::vector<std::wstring> v1;
        std::vector<std::wstring> v2;

        for (auto switchPtr : _switchList)
        {
            const SwitchBase& $switch = *switchPtr;

            if ($switch.GetShort().has_value())
            {
                v1.emplace_back(std::format(L"{}, {}",
                    $switch.GetName(), $switch.GetShort().value()));
            }
            else
            {
                v1.emplace_back($switch.GetName());
            }

            v2.emplace_back($switch.GetDescription());
        }

        PrintTwoColumns(v1, v2, std::wstring(2, L' '));
    }

    // Commands
    if (!_subcommands.empty())
    {
        os << L"\n";
        os << UtilGetResourceString(MA_STRING_COMMAND_HELP_COMMANDS) << L":\n";

        std::vector<std::wstring> v1;
        std::vector<std::wstring> v2;

        for (auto commandPtr : _subcommandList)
        {
            const CommandBase& command = *commandPtr;

            if (command._rest.GetParameter() != NullParameter)
            {
                v1.emplace_back(std::format(L"{} <{}>",
                    command._name, command._rest.GetParameterName()));
            }
            else
            {
                v1.emplace_back(command._name);
            }

            v2.emplace_back(command._description);
        }

        PrintTwoColumns(v1, v2, std::wstring(2, L' '));
    }

    // Run 'monika [command] --help' for more information on a command.
    if (_parentCommand == nullptr && !_subcommands.empty())
    {
        os << L"\n";
        os << std::vformat(
            UtilGetResourceString(MA_STRING_COMMAND_HELP_HINT_SUBCOMMAND),
            std::make_wformat_args(
                std::format(
                    L"{} [{}] {}",
                    _name,
                    UtilGetResourceString(MA_STRING_COMMAND_HELP_PLACEHOLDER_COMMAND),
                    UtilGetResourceString(MA_STRING_COMMAND_SWITCH_HELP_NAME)
                )
            )
        ) << L"\n";
    }

    std::flush(os);
}
