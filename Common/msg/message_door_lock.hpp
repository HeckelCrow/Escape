#pragma once
#include "message_format.hpp"

constexpr u32 latchlock_timeout       = 200;
constexpr u32 latchlock_timeout_retry = 1000;

enum class LockState : u8
{
    Locked,
    Open,
    SoftLock,
};

enum class LatchLockState : u8
{
    Unpowered,
    ForceOpen,
};

struct DoorLockCommand
{
    DoorLockCommand() {}

    MessageHeader
    getHeader(ClientId client_id)
    {
        return MessageHeader{MessageType::DoorLockCommand};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(lock_door, s);
        Serialize(lock_cave, s);
        Serialize(lock_tree, s);
    }
    LockState      lock_door = LockState::Open;
    LockState      lock_cave = LockState::Open;
    LatchLockState lock_tree = LatchLockState::Unpowered;
};

struct DoorLockStatus
{
    DoorLockStatus() {}

    MessageHeader
    getHeader(ClientId client_id)
    {
        return MessageHeader{MessageType::DoorLockStatus};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(lock_door, s);
        Serialize(lock_cave, s);
        Serialize(lock_tree, s);
    }

    LockState      lock_door           = LockState::Open;
    LockState      lock_cave           = LockState::Open;
    LatchLockState lock_tree           = LatchLockState::Unpowered;
    u32            last_tree_open_time = 0;
};
