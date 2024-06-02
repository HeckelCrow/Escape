#pragma once
#include "alias.hpp"
#include "client.hpp"
#include "msg/message_ring_dispenser.hpp"

struct RingDispenser
{
    RingDispenser() {}

    void receiveMessage(Client& client, const RingDispenserStatus& msg,
                        bool print);
    void update(Client& client);

    RingDispenserCommand command;
    RingDispenserStatus  last_status;
};
