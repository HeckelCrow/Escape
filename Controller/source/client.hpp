#pragma once
#include "alias.hpp"
#include "time.hpp"
#include "msg/connection.hpp"

constexpr Duration client_timeout_duration = Milliseconds(1200);
constexpr Duration heartbeat_period        = Milliseconds(700);
constexpr Duration command_resend_period   = Milliseconds(100);

struct Client
{
    /*
       If a client times out, it is deconnected.
    */
    bool
    timeout()
    {
        return (Clock::now() - time_last_message_received
                >= client_timeout_duration);
    }

    /*
       Heartbeat timeout means we haven't heard from a client for some time and
       we should send a message to see if we get a response.
    */
    bool
    heartbeatTimeout()
    {
        auto now = Clock::now();
        // We have to check both time_command_sent and
        // time_last_message_received because targets sends messages on its own
        // which makes the server stop sending heartbeats. We need to check
        // time_last_message_received too in case messages get lost.
        return (now - time_command_sent >= heartbeat_period)
               || (now - time_last_message_received >= heartbeat_period);
    }

    /*
       When we don't get a response from a client, we should wait until
       resendTimeout to try to send a message again.
    */
    bool
    resendTimeout()
    {
        return (Clock::now() - time_command_sent >= command_resend_period);
    }

    bool connected = false;

    Connection connection;
    Timepoint  time_last_message_received;
    Timepoint  time_command_sent;
};
