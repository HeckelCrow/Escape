#pragma once
#include "message_format.hpp"

enum class RingDispenserState
{
    DetectRings,
    ForceDeactivate,
    ForceActivate,
};

struct RingDispenserCommand
{
    RingDispenserCommand() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::RingDispenserCommand};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(ask_for_ack, s);
        Serialize(state, s);
        Serialize(rings_detected, s);
    }

    u8                 ask_for_ack    = 0;
    RingDispenserState state          = RingDispenserState::DetectRings;
    u32                rings_detected = 0;
};

struct RingDispenserStatus
{
    RingDispenserStatus() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::RingDispenserStatus};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(ask_for_ack, s);
        Serialize(state, s);
        Serialize(rings_detected, s);
    }

    u8                 ask_for_ack    = 0;
    RingDispenserState state          = RingDispenserState::DetectRings;
    u32                rings_detected = 0;
};
