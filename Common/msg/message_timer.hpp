#pragma once
#include "message_format.hpp"

struct TimerCommand
{
    TimerCommand() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::TimerCommand};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(paused, s);
        Serialize(time_left, s);
    }

    u8  paused    = false;
    s32 time_left = 0; // milliseconds
};

struct TimerStatus
{
    TimerStatus() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::TimerStatus};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(paused, s);
        Serialize(time_left, s);
    }

    u8  paused    = false;
    s32 time_left = 0; // milliseconds
};
