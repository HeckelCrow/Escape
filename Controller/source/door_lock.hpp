#pragma once
#include "alias.hpp"
#include "client.hpp"
#include "msg/message_door_lock.hpp"

struct DoorLock
{
    DoorLock() {}

    void receiveMessage(Client& client, const DoorLockStatus& msg, bool print);

    void update(Client& client);

    DoorLockCommand command;
    DoorLockStatus  last_status;
};