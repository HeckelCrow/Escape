#pragma once
#include <fmt/printf.h>
#include <fmt/color.h>
#include <iostream>

// TODO: Use std::print (C++23)?

template<typename... Args>
inline void
Print(const fmt::string_view str, Args&&... args)
{
    fmt::vprint(str, fmt::make_format_args(args...));
}

template<typename... Args>
inline void
PrintError(const fmt::string_view str, Args&&... args)
{
    fmt::print(stdout, fg(fmt::color::red), str, args...);
}

template<typename... Args>
inline void
PrintWarning(const fmt::string_view str, Args&&... args)
{
    fmt::print(stdout, fg(fmt::color::orange), str, args...);
}

template<typename... Args>
inline void
PrintSuccess(const fmt::string_view str, Args&&... args)
{
    fmt::print(stdout, fg(fmt::color::lime_green), str, args...);
}