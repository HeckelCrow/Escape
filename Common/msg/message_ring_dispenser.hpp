#pragma once
#include "message_format.hpp"

constexpr u32 activation_duration = 3000;

struct RingDispenserCommand
{
    RingDispenserCommand() {}

    MessageHeader
    getHeader(ClientId client_id)
    {
        return MessageHeader{MessageType::RingDispenserCommand};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(activate, s);
    }

    u8 activate = 0;
};

struct RingDispenserStatus
{
    RingDispenserStatus() {}

    MessageHeader
    getHeader(ClientId client_id)
    {
        return MessageHeader{MessageType::RingDispenserStatus};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(activated, s);
    }
    u8 activated = 0;
};
