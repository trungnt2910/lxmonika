#pragma once

#include <iosfwd>
#include <optional>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Switch.h"

class CommandBase
{
    template <typename T>
    friend class Command;
private:
    const std::wstring_view _name;
    const std::wstring_view _description;
    std::vector<const SwitchBase*> _switchList;
    std::unordered_map<std::wstring_view, const SwitchBase*> _switches;
    const SwitchBase& _rest;
    std::vector<const CommandBase*> _subcommandList;
    std::unordered_map<std::wstring_view, const CommandBase*> _subcommands;
    const CommandBase* _parentCommand;
    bool _shouldPrintHelp = false;
    Switch<bool> _helpSwitch;
protected:
    CommandBase(int name, int description,
        const SwitchBase& rest,
        const CommandBase* parentCommand = nullptr);

    virtual int Execute() const = 0;

    void AddSwitch(const SwitchBase& $switch);
    void AddCommand(const CommandBase& command);
public:
    virtual int Run(int& argc, wchar_t**& argv) const;

    void PrintHelp(std::wostream& os) const;

    const std::wstring_view& GetName() const { return _name; }
    const std::wstring_view& GetParameterName() const { return _rest.GetParameterName(); }
};

template <typename T = std::remove_cv_t<decltype(std::ignore)>>
class Command: public CommandBase
{
protected:
    Command(int name, int description,
        const Switch<T>& rest,
        const CommandBase* parentCommand = nullptr)
        : CommandBase(name, description, rest, parentCommand) { }
};
