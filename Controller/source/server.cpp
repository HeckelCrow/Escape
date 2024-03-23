#include "server.hpp"
#include "print.hpp"

// #define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// #define NOUSER
#include <Windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")

auto multicast_endpoint =
    Endpoint(asio::ip::make_address("239.255.0.1"), 55872);

Str
AsioErrorToUtf8(asio::error_code error)
{
    Str  ansi_str    = error.message();
    s32  size_needed = MultiByteToWideChar(CP_ACP, 0, &ansi_str[0],
                                           (int)ansi_str.size(), NULL, 0);
    WStr wstr;
    wstr.resize(size_needed);
    MultiByteToWideChar(CP_ACP, 0, &ansi_str[0], (int)ansi_str.size(), &wstr[0],
                        size_needed);

    Str utf8;
    size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                                      (int)wstr.size(), nullptr, 0, NULL, NULL);
    utf8.resize(size_needed);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &utf8[0],
                        size_needed, NULL, NULL);

    return utf8;
}

Str
WideCharToUtf8(const wchar_t* wide, s32 count)
{
    std::string utf8;
    if (!count)
    {
        return "";
    }
    s32 size_needed =
        WideCharToMultiByte(CP_UTF8, 0, wide, count, nullptr, 0, NULL, NULL);
    utf8.resize(size_needed);
    if (!size_needed)
    {
        // This shouldn't happen
        return "";
    }
    WideCharToMultiByte(CP_UTF8, 0, wide, count, &utf8[0], size_needed, NULL,
                        NULL);
    if (count < 0)
        utf8.resize(utf8.size() - 1); // remove '\0'
    return utf8;
}

bool
InitServer(Server& server)
{
    ULONG family = AF_INET; // IPv4
    ULONG flags  = GAA_FLAG_SKIP_DNS_SERVER;
    //| GAA_FLAG_INCLUDE_GATEWAYS
    //              | GAA_FLAG_INCLUDE_ALL_INTERFACES;
    ULONG size = 0;
    auto  ret  = GetAdaptersAddresses(family, flags, nullptr, nullptr, &size);
    if (ret != ERROR_BUFFER_OVERFLOW)
    {
        PrintError("GetAdaptersAddresses failed with error: {}\n", ret);
        return false;
    }
    std::vector<u8> working_buffer(size);
    ret = GetAdaptersAddresses(family, flags, nullptr,
                               (IP_ADAPTER_ADDRESSES*)working_buffer.data(),
                               &size);
    if (ret != NO_ERROR)
    {
        PrintError("GetAdaptersAddresses failed with error: {}\n", ret);
        return false;
    }

    std::vector<Endpoint> endpoints;
    for (IP_ADAPTER_ADDRESSES* address =
             (IP_ADAPTER_ADDRESSES*)working_buffer.data();
         address != NULL; address = address->Next)
    {
        Print("\nAdapter name: {}\n", address->AdapterName);
        Print("FriendlyName: {}\n", WideCharToUtf8(address->FriendlyName, -1));
        Print("Description: {}\n", WideCharToUtf8(address->Description, -1));
        Print("IfIndex: {}\n", address->IfIndex);
        Print("Flags: {:04X}\n", address->Flags);
        Print("Mtu: {}\n", address->Mtu);
        Print("IfType: {}\n",
              (address->IfType == IF_TYPE_IEEE80211)       ? "Wifi" :
              (address->IfType == IF_TYPE_ETHERNET_CSMACD) ? "Ethernet" :
                                                             "Other");

        // Print("PhysicalAddress: ");
        // for (u32 i = 0; i < address->PhysicalAddressLength; i++)
        //{
        //     Print("{}.", (u8)address->PhysicalAddress[i]);
        // }
        // Print("\n");

        if (address->IfType != IF_TYPE_IEEE80211
            && address->IfType != IF_TYPE_ETHERNET_CSMACD)
        {
            // Not Wifi or Ethernet
            continue;
        }

        Print("UnicastAddresses: \n");
        for (auto unicast = address->FirstUnicastAddress; unicast != NULL;
             unicast      = unicast->Next)
        {
            auto addr = (sockaddr_in*)unicast->Address.lpSockaddr;
            auto asio_address =
                asio::ip::address_v4(ntohl(addr->sin_addr.s_addr));
            // u16 asio_port = ntohs(addr->sin_port);
            Print("{}\n", asio_address.to_string());
            endpoints.emplace_back(asio_address, 0);
        }
        Print("AnycastAddresses: \n");
        for (auto anycast = address->FirstAnycastAddress; anycast != NULL;
             anycast      = anycast->Next)
        {
            auto addr = (sockaddr_in*)anycast->Address.lpSockaddr;
            auto asio_address =
                asio::ip::address_v4(ntohl(addr->sin_addr.s_addr));
            Print("{}\n", asio_address.to_string());
        }
        Print("MulticastAddresses: \n");
        for (auto multicast = address->FirstMulticastAddress; multicast != NULL;
             multicast      = multicast->Next)
        {
            auto addr = (sockaddr_in*)multicast->Address.lpSockaddr;
            auto asio_address =
                asio::ip::address_v4(ntohl(addr->sin_addr.s_addr));
            Print("{}\n", asio_address.to_string());
        }
    }

    asio::error_code error;
    for (auto& endpoint : endpoints)
    {
        server.sockets.push_back(std::make_shared<Socket>(server.io_context));
        auto socket = server.sockets.back().get();
        socket->open(asio::ip::udp::v4(), error);
        if (error)
        {
            PrintError("Error: socket.open: {}\n", error.message());
            return false;
        }
        socket->bind(endpoint, error);
        if (error)
        {
            // Print("Error: socket.bind: {}\n", error.message());
            //  return false;
            server.sockets.pop_back();
            continue;
        }

        // server.socket.open(asio::ip::udp::v4(), error);
        // if (error)
        //{
        //     PrintError("Error: socket.open: {}\n", AsioErrorToUtf8(error));
        //     return false;
        // }
        // server.socket.bind(Endpoint(asio::ip::udp::v4(), 0), error);
        // if (error)
        //{
        //     PrintError("Error: socket.bind: {}\n", AsioErrorToUtf8(error));
        //     return false;
        // }

        PrintSuccess("- {} port {}\n", endpoint.address().to_string(),
                     socket->local_endpoint().port());
    }

    // PrintSuccess("port {}\n", server.socket.local_endpoint().port());

    // Receive multicast packets:
    // Endpoint listen_endpoint(asio::ip::make_address("0.0.0.0"),
    //                          udp_broadcast_port);
    // socket.open(listen_endpoint.protocol(), error);
    // socket.set_option(asio::ip::udp::socket::reuse_address(true));
    // socket.bind(listen_endpoint);

    // socket.set_option(asio::ip::multicast::join_group(multicast_address));

    // socket.open(asio::ip::udp::v4(), error);
    // if (error)
    //{
    //     PrintError("Error: socket.open: {}\n", AsioErrorToUtf8(error));
    //     return -1;
    // }

    return true;
}

void
TerminateServer(Server& server)
{
    for (auto& socket : server.sockets)
        socket->close();
    server.sockets.clear();
    server.buffer.clear();
    server.buffer.shrink_to_fit();
}

void
SendPacketMulticast(Server& server, Serializer& s)
{
    asio::error_code error;
    BufferPtr        buffer = {s.full_buffer.start, s.buffer.start};

    for (auto& socket : server.sockets)
    {
        socket->send_to(asio::buffer(buffer.start, buffer.end - buffer.start),
                        multicast_endpoint, 0, error);
        if (error)
        {
            PrintError("Error: socket.send_to multicast_endpoint: {}\n",
                       AsioErrorToUtf8(error));
            return;
        }
    }
}

void
SendPacket(Connection& connection, Serializer& s)
{
    asio::error_code error;
    BufferPtr        buffer = {s.full_buffer.start, s.buffer.start};
    connection.socket->send_to(
        asio::buffer(buffer.start, buffer.end - buffer.start),
        connection.endpoint, 0, error);
    if (error)
    {
        PrintError("Error: socket.send_to: {}\n", AsioErrorToUtf8(error));
        return;
    }
}

Message
ReceiveMessage(Server& server)
{
    Message          msg;
    asio::error_code error;
    for (auto& socket : server.sockets)
    {
        auto bytes_available = socket->available(error);
        if (error)
        {
            PrintError("Error: socket.available: {}\n", AsioErrorToUtf8(error));
            return {};
        }
        if (bytes_available != 0)
        {
            if (server.buffer.size() < bytes_available)
            {
                server.buffer.resize(bytes_available);
            }
            u64 size = socket->receive_from(asio::buffer(server.buffer),
                                            msg.from.endpoint, 0, error);
            if (error)
            {
                PrintError("Error: socket.receive: {}\n",
                           AsioErrorToUtf8(error));
                return {};
            }

            if (size >= 2)
            {
                msg.deserializer =
                    Serializer(SerializerMode::Deserialize,
                               BufferPtr{server.buffer.data(), (u32)size});
                msg.header.serialize(msg.deserializer);
                msg.from.socket = socket;
            }
            break;
        }
    }
    return msg;
}
