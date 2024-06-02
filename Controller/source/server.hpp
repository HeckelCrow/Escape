#pragma once
#include "alias.hpp"
#include "msg/message_format.hpp"

#include <asio.hpp>
#include <vector>

constexpr u32 udp_packet_size = 1024;

struct Server
{
    asio::io_context       io_context;
    std::vector<SocketPtr> sockets;

    std::vector<u8> buffer;
};

bool    InitServer(Server& server);
void    TerminateServer(Server& server);
void    SendPacketMulticast(Server& server, Serializer& s);
void    SendPacket(Connection& connection, Serializer& s);
Message ReceiveMessage(Server& server);