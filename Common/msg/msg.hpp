#pragma once
#include "alias.hpp"
#include <IPAddress.h>
#include <WiFiUdp.h>

struct Connection
{
    Connection() {}
    Connection(IPAddress addr_in, u16 port_in) : address(addr_in), port(port_in)
    {}
    IPAddress address;
    u16       port = 0;
};
#include "message_format.hpp"

extern WiFiUDP    udp;
extern Connection server_connection;

constexpr u32 udp_packet_size = 1024;
extern u8     packet_buffer[udp_packet_size];

enum class WifiState
{
    WifiOff,
    WaitingForWifi,
    StartMulticast,
    WaitingForMulticast,
    Connected,
};
extern WifiState wifi_state;

void    StartWifi(bool access_point = false);
Message ReceiveMessage();

void WifiScan();