#pragma once
#include "alias.hpp"
#include "hashtable.hpp"
#include "print.hpp"
#include <charconv>

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

inline StrPtr
ReadLine(StrPtr str, u64& i)
{
    u64 start = i;
    u64 end   = i;
    while (i < str.size())
    {
        if (str[i] != '\r' && str[i] != '\n')
        {
            end = i + 1;
        }

        if (str[i] == '\n')
        {
            i++;
            break;
        }

        i++;
    }

    return StrPtr(str.data() + start, end - start);
}

template<typename T>
inline bool
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

template<>
inline bool
TryRead(StrPtr str, u64& i, bool& value)
{
    StrPtr word;
    if (TryReadWord(str, i, word))
    {
        if (word == "true")
        {
            value = true;
            return true;
        }
        else if (word == "false")
        {
            value = false;
            return true;
        }
        else
        {
            PrintWarning("Invalid bool value \"{}\"", word);
        }
    }

    return false;
}

template<typename T>
inline T
Read(StrPtr str, u64& i, T default_value)
{
    T value = 0;
    if (TryRead(str, i, value))
    {
        return value;
    }
    return default_value;
}

struct Settings
{
    Hashtable<Str, Str> values;
};

extern Settings settings;

void LoadSettings(Path path);
void SaveSettings(Path path);

template<typename T>
bool
LoadSettingValue(const Str& name, T& value)
{
    auto it = settings.values.find(name);
    if (it == settings.values.end())
    {
        PrintWarning("Can't find setting {}\n", name);
        return false;
    }
    u64 i = 0;
    return TryRead(it->second, i, value);
}

template<typename T>
void
SaveSettingValue(const Str& name, T value)
{
    Str  value_str = fmt::format("{}", value);
    auto it        = settings.values.insert_or_assign(name, value_str);
}