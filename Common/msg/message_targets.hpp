#pragma once
#include "message_format.hpp"

constexpr u8 target_count = 4;

struct TargetsCommand
{
    TargetsCommand()
    {
        for (auto& h : set_hitpoints)
        {
            h = -1;
        }
    }

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::TargetsCommand};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(ask_for_ack, s);
        Serialize(enable, s);
        Serialize(talk, s);

        auto target_count_msg = target_count;
        Serialize(target_count_msg, s);
        // If we receive more targets than we have we don't deserialize them.
        if (target_count < target_count_msg)
            target_count_msg = target_count;

        for (u32 i = 0; i < target_count_msg; i++)
        {
            Serialize(hitpoints[i], s);
            Serialize(set_hitpoints[i], s);
        }
    }

    u8 ask_for_ack                 = 0;
    u8 enable                      = 0;
    u8 talk                        = 0;
    s8 hitpoints[target_count]     = {0};
    s8 set_hitpoints[target_count] = {0};
};

struct TargetsStatus
{
    TargetsStatus() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::TargetsStatus};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(ask_for_ack, s);
        Serialize(enabled, s);
        Serialize(talk, s);

        auto target_count_msg = target_count;
        Serialize(target_count_msg, s);
        // If we receive more targets than we have we don't deserialize them.
        if (target_count < target_count_msg)
            target_count_msg = target_count;

        for (u32 i = 0; i < target_count_msg; i++)
        {
            Serialize(hitpoints[i], s);
        }
    }

    u8 ask_for_ack             = 0;
    u8 enabled                 = 0;
    u8 talk                    = 0;
    s8 hitpoints[target_count] = {0};
};
