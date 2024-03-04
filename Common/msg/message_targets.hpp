#pragma once
#include "message_format.hpp"

struct TargetsCommand
{
    TargetsCommand() {}

    MessageHeader
    getHeader(ClientId client_id)
    {
        return MessageHeader{MessageType::TargetsCommand};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(enable, s);
    }

    u8 enable = 0;
};

struct TargetsStatus
{
    TargetsStatus() {}

    MessageHeader
    getHeader(ClientId client_id)
    {
        return MessageHeader{MessageType::TargetsStatus};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(enabled, s);
    }

    u8 enabled = 0;
};
