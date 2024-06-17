#pragma once
#include "console.hpp"

#include <fmt/printf.h>
#include <fmt/color.h>
#include <iostream>

// TODO: Use std::print (C++23)?

template<typename... Args>
inline void
Print(const fmt::string_view str, Args&&... args)
{
    const auto formated = fmt::vformat(str, fmt::make_format_args(args...));
    std::fputs(formated.data(), stdout);
    console.messages.push_back({ConsoleMessageType::Info, std::move(formated)});
}

template<typename... Args>
inline void
PrintError(const fmt::string_view str, Args&&... args)
{
    const auto formated = fmt::vformat(str, fmt::make_format_args(args...));
    std::fputs(formated.data(), stdout);
    console.messages.push_back(
        {ConsoleMessageType::Error, std::move(formated)});
}

template<typename... Args>
inline void
PrintWarning(const fmt::string_view str, Args&&... args)
{
    const auto formated = fmt::vformat(str, fmt::make_format_args(args...));
    std::fputs(formated.data(), stdout);
    console.messages.push_back(
        {ConsoleMessageType::Warning, std::move(formated)});
}

template<typename... Args>
inline void
PrintSuccess(const fmt::string_view str, Args&&... args)
{
    const auto formated = fmt::vformat(str, fmt::make_format_args(args...));
    std::fputs(formated.data(), stdout);
    console.messages.push_back(
        {ConsoleMessageType::Success, std::move(formated)});
}