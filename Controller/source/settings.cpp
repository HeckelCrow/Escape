#include "settings.hpp"
#include "file_io.hpp"
#include "print.hpp"

Settings settings;

Settings
LoadSettings(Path path)
{
    Settings settings;
    Str      file = ReadBinaryFile(path);
    StrPtr   str  = file;
    u64      i    = 0;

    while (i < str.size())
    {
        ReadAllSpaces(str, i);
        StrPtr name;
        if (!TryReadWord(str, i, name))
            break;

        ReadAllSpaces(str, i);
        if (name == "threshold")
        {
            u8 target;
            if (TryRead(str, i, target))
            {
                if (target > 0 && target <= target_count)
                {
                    ReadAllSpaces(str, i);
                    settings.target_thresholds[target - 1] =
                        Read(str, i, target_default_threshold);
                    Print("Threshold {} = {}\n", target,
                          settings.target_thresholds[target - 1]);
                }
                else
                {
                    PrintWarning("Settings contains invalid target {}\n",
                                 target);
                }
            }
        }
    }
    return settings;
}
