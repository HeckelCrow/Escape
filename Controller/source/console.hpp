#pragma once
#include "alias.hpp"

enum class ConsoleMessageType
{
    Info,
    Error,
    Warning,
    Success,
};

struct ConsoleMessage
{
    ConsoleMessageType type;
    Str                text;
};

struct Console
{
    Str                              input_buffer = Str(1024, '\0');
    std::vector<std::pair<Str, Str>> matching_commands;

    std::vector<ConsoleMessage> messages;

    std::vector<Str> history;
    u32              history_index;
    Str              buffered_command;

    bool open = true;
};

extern Console console;

void DrawConsole();