#pragma once
#include "alias.hpp"
#include "print.hpp"
#include <tuple>
#include <optional>
#include <charconv>
#include <functional>

// From https://github.com/RippeR37/SLACC

template<typename T>
inline std::optional<T>
StringToArg(StrPtr str)
{
    if (str.empty())
    {
        PrintError("Missing argument\n");
        return {};
    }

    T    x   = 0;
    auto err = std::from_chars(str.data(), str.data() + str.size(), x);
    if (err.ec == std::errc::invalid_argument)
    {
        PrintError("Invalid argument \"{}\"\n", str);
        return {};
    }
    else if (err.ec == std::errc::result_out_of_range)
    {
        PrintError("Argument is out of range \"{}\"\n", str);
        return {};
    }
    return x;
}

template<u32 N>
struct ApplyArgRecursive
{
    template<typename Func, typename Tuple, typename... Args>
    static void
    apply(const Func& func, const Tuple& tuple, const Args&... args)
    {
        ApplyArgRecursive<N - 1>::apply(func, tuple, std::get<N - 1>(tuple),
                                        args...);
    }
};

template<>
struct ApplyArgRecursive<0>
{
    template<typename Func, typename Tuple, typename... Args>
    static void
    apply(const Func& func, const Tuple& /*tuple*/, const Args&... args)
    {
        func(args...);
    }
};

template<typename Func, typename... Args>
inline void
ApplyArg(const Func& func, const std::tuple<Args...>& args)
{
    ApplyArgRecursive<std::tuple_size<std::tuple<Func, Args...>>::value
                      - 1>::apply(func, args);
}

template<typename T, typename U, typename... Args>
std::optional<std::tuple<T, U, Args...>> ArgToTupleSplitter(StrPtr str);

template<typename T>
std::optional<std::tuple<T>> ArgToTupleSplitter(StrPtr str);

template<typename... Ts>
inline bool
parse(const std::function<void(Ts...)>& func, StrPtr args)
{
    auto tupledArgs = ArgToTupleSplitter<Ts...>(args);
    if (tupledArgs)
    {
        ApplyArg(func, tupledArgs.value());
        return true;
    }
    return false;
}

/**
 * Basic parser - Specialization for commands with 0 arguments.
 *                Any passed arguments are ignored.
 */
template<>
inline bool
parse<>(const std::function<void()>& func, StrPtr /*args*/)
{
    func();
    return true;
}

/**
 * Basic argument parsing - returns tuple <arg0, rest>
 */
inline std::tuple<StrPtr, StrPtr>
parseFirstArg(StrPtr args)
{
    std::size_t i = 0, j = 0,
                k; // first argument is substring of range [i, j] in 'args'

    // Skip whitespaces before argument
    while (i < args.length() && std::isspace(args[i]))
        ++i;

    // If all characters are whitespaces, return empty argument
    if (i == args.length())
        return std::make_tuple(StrPtr{}, StrPtr{});

    if (args[i] == '\"')
    {
        // If argument starts with opening ", then look for closing "
        j = ++i;

        while (j < args.length() && args[j] != '\"')
            ++j;
    }
    else
    {
        // If argument starts from anything else, it ends upon first
        // whitespace
        j = i + 1;

        while (j < args.length() && !std::isspace(args[j]))
            ++j;
    }

    // Verify that k is in [0, args.length()] range and remove whitespaces
    // from left side
    k = std::min(j + 1, args.length());
    while (k < args.length() && isspace(args[k]))
        ++k;

    return std::make_tuple(args.substr(i, j - i), args.substr(k));
}

/**
 * ArgToTupleSplitter<T, U, Args...> - splits arguments in StrPtr
 * to std::tuple<T, U, Args...> between whitespaces. Last argument uses
 * what's left in argument list.
 *
 * T - current argument type
 * U - next argument type
 * Args... - possible next argument types
 */
template<typename T, typename U, typename... Args>
inline std::optional<std::tuple<T, U, Args...>>
ArgToTupleSplitter(StrPtr str)
{
    std::tuple<StrPtr, StrPtr> parsedArgs = parseFirstArg(str);
    StrPtr                     this_arg   = std::get<0>(parsedArgs);
    StrPtr                     rest_args  = std::get<1>(parsedArgs);

    auto arg = StringToArg<T>(this_arg);
    if (arg)
    {
        return std::tuple_cat(
            std::make_tuple(std::move(arg.value())),  // this argument
            ArgToTupleSplitter<U, Args...>(rest_args) // rest of arguments
        );
    }
    return {};
}

/**
 * ArgToTupleSplitter<T> - specialization to ArgToTupleSplitter<T, U,
 * Args...> which handles last argument. This method uses as argument's
 * source whole string, not only till first whitespace!
 *
 * T - current argument type
 */
template<typename T>
inline std::optional<std::tuple<T>>
ArgToTupleSplitter(StrPtr str)
{
    auto arg = StringToArg<T>(str);
    if (arg)
    {
        return std::make_tuple(std::move(arg.value()));
    }
    return {};
}

struct Command
{
    Str                         name;
    std::vector<Str>            argument_names;
    std::function<bool(StrPtr)> function;
};

enum class ConsoleError
{
    Success,
    UnknownCommand,
    InvalidArguments
};

inline Str
Concatenate(std::vector<Str> vec, StrPtr separator)
{
    Str res;
    u64 vec_size = vec.size();
    if (vec_size)
    {
        for (u64 i = 0; i < vec_size - 1; i++)
        {
            res += vec[i];
            res += separator;
        }
        res += vec.back();
    }
    return res;
}

struct Commands
{
    // Bind command with function using custom parser
    template<typename Func, typename FuncParser>
    void
    bind(StrPtr name, std::vector<StrPtr> argument_names, const Func& func,
         const FuncParser& parser)
    {
        auto it =
            std::find_if(commands.begin(), commands.end(),
                         [=](const Command& cmd) { return cmd.name == name; });
        if (it != commands.end())
        {
            it->function = std::bind(parser, func, std::placeholders::_1);
        }
        else
        {
            Command new_command = {
                Str(name), {}, std::bind(parser, func, std::placeholders::_1)};
            for (auto arg_name : argument_names)
            {
                new_command.argument_names.push_back(Str(arg_name));
            }
            commands.push_back(std::move(new_command));
        }
    }

    // Bind command with function using default (basic) parser - std::function
    // wrapper
    template<typename... FuncArgs>
    void
    bind(StrPtr name, std::vector<StrPtr> argument_names,
         const std::function<void(FuncArgs...)>& func)
    {
        bind(name, argument_names, func, parse<FuncArgs...>);
    }

    // Bind command with function using default (basic) parser - function
    // pointer
    template<typename... FuncArgs>
    void
    bind(StrPtr name, std::vector<StrPtr> argument_names,
         void (*func)(FuncArgs...))
    {
        bind(name, argument_names, func, parse<FuncArgs...>);
    }

    // Execute command with given arguments
    ConsoleError
    execute(StrPtr command)
    {
        // Command name
        std::size_t name_pos = command.find_first_not_of(" \t\r\n");
        std::size_t name_end = command.find_first_of(" \t\r\n", name_pos);
        std::size_t name_len = 0;
        if (name_pos != std::string::npos)
        {
            if (name_end == std::string::npos)
            {
                name_len = command.length() - name_pos;
            }
            else
            {
                name_len = name_end - name_pos;
            }
        }
        else
        {
            name_pos = 0;
        }

        // Arguments
        std::size_t args_pos =
            std::min(name_pos + name_len + 1, command.length());
        std::size_t args_end = command.find_last_not_of(" \t\r\n");
        std::size_t args_len = std::max(args_pos, args_end) - args_pos + 1;

        // Values
        StrPtr cmd_name = command.substr(name_pos, name_len);
        StrPtr cmd_args = command.substr(args_pos, args_len);

        // Execution
        auto it = std::find_if(
            commands.begin(), commands.end(),
            [=](const Command& cmd) { return cmd.name == cmd_name; });
        if (it != commands.end())
        {
            if (it->function(cmd_args))
            {
                return ConsoleError::Success;
            }
            else
            {
                return ConsoleError::InvalidArguments;
            }
        }
        else
        {
            return ConsoleError::UnknownCommand;
        }
    }

    std::vector<std::pair<Str, Str>>
    complete(StrPtr command)
    {
        std::size_t name_pos = command.find_first_not_of(" \t\r\n");
        std::size_t name_end = command.find_first_of(" \t\r\n", name_pos);
        std::size_t name_len = 0;
        if (name_pos == std::string::npos || name_end != std::string::npos)
        {
            return {};
        }
        name_len = command.length() - name_pos;

        StrPtr partial_name = command.substr(name_pos, name_len);

        std::vector<std::pair<Str, Str>> matching_commands;
        for (const auto& cmd : commands)
        {
            if (cmd.name.rfind(partial_name, 0) != std::string::npos)
            {
                matching_commands.push_back(
                    {cmd.name, Concatenate(cmd.argument_names, " ")});
            }
        }
        return matching_commands;
    }

    std::vector<Command> commands;
};

extern Commands console_commands;

template<typename T>
inline void
RegisterConsoleCommand(StrPtr name, std::vector<StrPtr> arg_names, T function)
{
    console_commands.bind(name, arg_names, function);
}