#pragma once
#include "message_format.hpp"

struct TimerCommand
{
    TimerCommand() {}

    MessageHeader
    getHeader(ClientId client_id)
    {
        return MessageHeader{MessageType::TimerCommand};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(time_left, s);
    }

    s32 time_left = 0; // seconds
};

struct TimerStatus
{
    TimerStatus() {}

    MessageHeader
    getHeader(ClientId client_id)
    {
        return MessageHeader{MessageType::TimerStatus};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(time_left, s);
    }

    s32 time_left = 0; // seconds
};
