#include "settings.hpp"
#include "file_io.hpp"
#include "print.hpp"

Settings settings;

void
LoadSettings(Path path)
{
    settings     = {};
    Str    file  = ReadBinaryFile(path);
    StrPtr str   = file;
    u64    str_i = 0;

    while (str_i < str.size())
    {
        auto line = ReadLine(str, str_i);
        u64  i    = 0;

        ReadAllSpaces(line, i);
        StrPtr name;
        if (!TryReadWord(line, i, name))
            break;

        ReadAllSpaces(line, i);
        StrPtr value = line.substr(i);

        settings.values.insert_or_assign(Str(name), Str(value));
        Print("{} = {}\n", name, value);
    }
}

void
SaveSettings(Path path)
{
    Str str;
    for (auto& it : settings.values)
    {
        str += fmt::format("{} {}\n", it.first, it.second);
    }
    WriteFile(path, str);
}
