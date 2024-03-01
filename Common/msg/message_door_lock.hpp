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
        Serialize(tree_open_duration, s);
    }

    LockState lock_door = LockState::Open;
    LockState lock_cave = LockState::Open;
    // tree_open_duration goes up to latchlock_timeout_retry even if lock_tree
    // is set to Unpowered
    LatchLockState lock_tree          = LatchLockState::Unpowered;
    u32            tree_open_duration = 0;
};
