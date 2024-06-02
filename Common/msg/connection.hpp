#pragma once
#ifdef CONTROLLER
#    include <asio.hpp>

using Endpoint  = asio::ip::udp::endpoint;
using Socket    = asio::ip::udp::socket;
using SocketPtr = std::shared_ptr<Socket>;

struct Connection
{
    Connection() {}
    Endpoint  endpoint;
    SocketPtr socket;
};

#else

struct Connection
{
    Connection() {}
    Connection(IPAddress addr_in, u16 port_in) : address(addr_in), port(port_in)
    {}
    IPAddress address;
    u16       port = 0;
};

#endif