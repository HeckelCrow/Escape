#pragma once
#include "alias.hpp"

#include <asio.hpp>
#include <vector>

using Endpoint  = asio::ip::udp::endpoint;
using Socket    = asio::ip::udp::socket;
using SocketPtr = std::shared_ptr<Socket>;

constexpr u32 udp_packet_size = 1024;

struct Connection
{
    Connection() {}
    Endpoint  endpoint;
    SocketPtr socket;
};
#include "msg/message_format.hpp"

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