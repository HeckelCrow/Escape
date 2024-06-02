#pragma once
#include "alias.hpp"

#include "msg/message_targets.hpp"
#include <charconv>

struct Settings
{
    Settings()
    {
        for (auto& th : target_thresholds)
            th = target_default_threshold;
    }

    u16 target_thresholds[target_count] = {};
};

extern Settings settings;

Settings LoadSettings(Path path);

template<typename T>
bool
TryRead(StrPtr str, u64& i, T& value)
{
    auto [ptr, ec] =
        std::from_chars(str.data() + i, str.data() + str.size(), value);
    if (ec == std::errc())
    {
        i = ptr - str.data();
        return true;
    }
    return false;
}

template<typename T>
T
Read(StrPtr str, u64& i, T default_value)
{
    T value = 0;
    if (TryRead(str, i, value))
    {
        return value;
    }
    return default_value;
}

inline bool
ReadAllSpaces(StrPtr str, u64& i)
{
    bool found_space = false;
    while (i < str.size())
    {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\r' && str[i] != '\n')
        {
            break;
        }
        found_space = true;
        i++;
    }
    return found_space;
}

inline bool
TryReadWord(StrPtr str, u64& i, StrPtr& word)
{
    u64 start = i;
    while (i < str.size())
    {
        if (str[i] == ' ' || str[i] == '\t' || str[i] == '\r' || str[i] == '\n')
        {
            break;
        }
        i++;
    }
    u64 end = i;
    word    = StrPtr(str.data() + start, end - start);
    return word.size() != 0;
}
