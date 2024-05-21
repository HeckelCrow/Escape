#pragma once
#include "message_format.hpp"
#include <vector>

constexpr u8 target_count = 4;

struct TargetsCommand
{
    TargetsCommand()
    {
        for (auto& h : set_hitpoints)
        {
            h = -1;
        }
        for (auto& th : thresholds)
        {
            th = 10000;
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

        auto target_count_msg = target_count;
        Serialize(target_count_msg, s);
        // If we receive more targets than we have we don't deserialize them.
        if (target_count < target_count_msg)
            target_count_msg = target_count;

        for (u32 i = 0; i < target_count_msg; i++)
        {
            Serialize(hitpoints[i], s);
            Serialize(set_hitpoints[i], s);
            Serialize(thresholds[i], s);
        }
        Serialize(send_sensor_data, s);
    }

    u8  ask_for_ack                 = 0;
    u8  enable                      = 0;
    s8  hitpoints[target_count]     = {0};
    s8  set_hitpoints[target_count] = {0};
    u16 thresholds[target_count]    = {0};
    u8  send_sensor_data            = 0;
};

struct TargetsStatus
{
    TargetsStatus()
    {
        for (auto& th : thresholds)
        {
            th = 10000;
        }
    }

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

        auto target_count_msg = target_count;
        Serialize(target_count_msg, s);
        // If we receive more targets than we have we don't deserialize them.
        if (target_count < target_count_msg)
            target_count_msg = target_count;

        for (u32 i = 0; i < target_count_msg; i++)
        {
            Serialize(hitpoints[i], s);
            Serialize(thresholds[i], s);
        }
    }

    u8  ask_for_ack              = 0;
    u8  enabled                  = 0;
    s8  hitpoints[target_count]  = {0};
    u16 thresholds[target_count] = {0};
    s8  send_sensor_data         = -1;
};

struct TargetsGraph
{
    TargetsGraph() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::TargetsGraph};
    }

    void
    serialize(Serializer& s)
    {
        for (u16 j = 0; j < target_count; j++)
        {
            Serialize(buffer_count[j], s);
            auto* target_buffer = buffer[j];
            for (u16 i = 0; i < buffer_count[j]; i++)
            {
                Serialize(target_buffer[i], s);
            }
        }
    }

    static constexpr u16 buffer_max_count                       = 64;
    u16                  buffer[target_count][buffer_max_count] = {0};
    u16                  buffer_count[target_count]             = {0};
};