﻿#include "alias.hpp"
#include "print.hpp"
#include "scope_exit.hpp"

#define utf8(s) (char*)u8##s
#include <asio.hpp>

// #define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// #define NOUSER
#include <Windows.h> // Asio error message conversion
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <al.h>
#include <alext.h>
#include <sndfile.h>

#include <span>
#include <chrono>
#include <atomic>

using Endpoint  = asio::ip::udp::endpoint;
using Socket    = asio::ip::udp::socket;
using SocketPtr = std::unique_ptr<Socket>;

struct Connection
{
    Connection() {}
    Endpoint endpoint;
    Socket*  socket = nullptr;
};

#include "msg/message_format.hpp"
#include "msg/message_door_lock.hpp"

using Clock     = std::chrono::high_resolution_clock;
using Timepoint = std::chrono::time_point<Clock>;
using Duration  = std::chrono::microseconds;

ClientId this_client_id = ClientId::Server;

const char* client_names[(u32)ClientId::IdMax] = {
    "Invalid",      "Server", "Door Lock", "Key in Tree",
    "Cave Buttons", "Rings",  "Targets"};

constexpr Duration
Microseconds(u64 t)
{
    return std::chrono::microseconds(t);
}

constexpr Duration
Milliseconds(u64 t)
{
    return std::chrono::milliseconds(t);
}

void
GlfwErrorCallback(int error_code, const char* message)
{
    PrintError("GLFW error {}: {}\n", error_code, message);
}

GLFWwindow*
OpenWindow()
{
    Vec2s        window_size = Vec2s(400, 200);
    GLFWmonitor* monitor     = nullptr;
    Vec2s        monitor_pos = {S32_MIN, 0};
    // Select monitor
    s32  count    = 0;
    auto monitors = glfwGetMonitors(&count);
    if (monitors && count)
    {
        for (auto m : std::span{monitors, (size_t)count})
        {
            auto name = glfwGetMonitorName(m);
            s32  x, y;
            glfwGetMonitorPos(m, &x, &y);

            if (x > monitor_pos.x)
            {
                monitor_pos = Vec2s(x, y);
                monitor     = m;
            }
        }
    }
    else
    {
        PrintError("glfwGetMonitors failed.\n");
        return nullptr;
    }

    auto* mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);

    glfwWindowHint(GLFW_VISIBLE, false);

    GLFWwindow* window = glfwCreateWindow(window_size.x, window_size.y,
                                          "Escape Game", nullptr, nullptr);
    if (!window)
    {
        PrintError("glfwCreateWindow failed.\n");
        return nullptr;
    }

    Vec2s monitor_size = Vec2s(mode->width, mode->height);
    Vec2s window_pos   = monitor_pos + (monitor_size - window_size) / 2;
    glfwSetWindowPos(window, window_pos.x, window_pos.y);
    glfwMaximizeWindow(window);

    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    // glfwSetFramebufferSizeCallback(window, GlfwFrameBufferSizeCallback);
    // glfwSetKeyCallback(window, GlfwKeyCallback);
    // glfwSetCursorPosCallback(window, GlfwCursorPositionCallback);
    // glfwSetMouseButtonCallback(window, GlfwMouseButtonCallback);
    // glfwSetScrollCallback(window, GlfwScrollCallback);

    if (gladLoadGL())
    {
        Print("OpenGL {}.{}\n", GLVersion.major, GLVersion.minor);
    }
    else
    {
        PrintError("gladLoadGL failed.\n");
    }

    return window;
}

void
InitImgui(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style    = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    // style.Colors[ImGuiCol_WindowBg].w = 1.f;
    // style.Colors[ImGuiCol_WindowBg]   = ImColor(color_background);
    // style.Colors[ImGuiCol_ChildBg]    = style.Colors[ImGuiCol_WindowBg];
    // style.Colors[ImGuiCol_TabActive]  = ImColor(HexToColor(0xB33F28FF));
    // style.Colors[ImGuiCol_TabHovered] = ImColor(HexToColor(0xB36428FF));

    ImVector<ImWchar>        ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddText((char*)u8"▷◯");
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.BuildRanges(&ranges);

    auto data_path = Path("data/");
    auto font_path = (data_path / "fonts/JetBrainsMono-Regular.ttf").string();
    io.Fonts->AddFontFromFileTTF(font_path.c_str(), 17, nullptr, ranges.Data);
    io.Fonts->Build();
    static auto ini_path = (data_path / "imgui.ini").string();
    io.IniFilename       = ini_path.c_str();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void
TerminateImgui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void
ImguiStartFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Vec2f(0.0f, 0.0f));
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("Dockspace Window", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID            dockspace_id    = ImGui::GetID("Dockspace");
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode
                                         | ImGuiDockNodeFlags_AutoHideTabBar;
    /*
    ImGuiDockNodeFlags_NoDockingInCentralNode       = 1 << 2,
    ImGuiDockNodeFlags_PassthruCentralNode          = 1 << 3,
    ImGuiDockNodeFlags_NoSplit                      = 1 << 4,
    ImGuiDockNodeFlags_NoResize                     = 1 << 5,
    ImGuiDockNodeFlags_AutoHideTabBar
    */
    ImGui::DockSpace(dockspace_id, Vec2f(0.0f, 0.0f), dockspace_flags);

    ImGui::End();
}

void
ImguiEndFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

auto multicast_endpoint =
    Endpoint(asio::ip::make_address("239.255.0.1"), 55872);

constexpr u32 udp_packet_size = 1024;

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

struct Server
{
    asio::io_context io_context;
    // Socket           socket = Socket(io_context);
    std::vector<SocketPtr> sockets;

    std::vector<u8> buffer;
};
Server server;

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

        Print("PhysicalAddress: ");
        for (u32 i = 0; i < address->PhysicalAddressLength; i++)
        {
            Print("{}.", (u8)address->PhysicalAddress[i]);
        }
        Print("\n");

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
        server.sockets.push_back(std::make_unique<Socket>(server.io_context));
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
                msg.from.socket = socket.get();
            }
            break;
        }
    }
    return msg;
}

constexpr Duration client_timeout_duration = Milliseconds(1200);
constexpr Duration heartbeat_period        = Milliseconds(700);
constexpr Duration command_resend_period   = Milliseconds(100);

struct Client
{
    bool
    heartbeatTimeout()
    {
        return (Clock::now() - time_last_message_received >= heartbeat_period);
    }

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

bool
SelectableButton(const char* name, bool selected)
{
    if (selected)
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              (ImVec4)ImColor::HSV(3 / 7.f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              (ImVec4)ImColor::HSV(3 / 7.f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              (ImVec4)ImColor::HSV(3 / 7.f, 0.8f, 0.8f));
    }
    bool pressed = ImGui::Button(name);
    if (selected)
    {
        ImGui::PopStyleColor(3);
    }
    return pressed;
}

void
DrawLock(const char* name, LockState& cmd, const LockState status)
{
    ImGui::PushID(name);
    SCOPE_EXIT({ ImGui::PopID(); });
    if (SelectableButton(utf8("Déverrouiller"), cmd == LockState::Open))
    {
        cmd = LockState::Open;
    }
    ImGui::SameLine();
    if (SelectableButton(utf8("Verrouiller"), cmd == LockState::Locked))
    {
        cmd = LockState::Locked;
    }
    ImGui::SameLine();
    if (SelectableButton(utf8("Fermeture douce"), cmd == LockState::SoftLock))
    {
        cmd = LockState::SoftLock;
    }

    Vec4f color = ImGui::GetStyle().Colors[ImGuiCol_Text];
    if (status != cmd)
    {
        color = {0.9f, 0.45f, 0.1f, 1.f};
    }

    if (status == LockState::Open)
    {
        ImGui::TextColored(color, utf8("> Déverrouillée"));
    }
    else if (status == LockState::SoftLock)
    {
        ImGui::TextColored(color, utf8("> Fermeture douce"));
    }
    else if (status == LockState::Locked)
    {
        ImGui::TextColored(color, utf8("> Verrouillée"));
    }
}

void
DrawLatchLock(LatchLockState& cmd, const LatchLockState status,
              const u32 tree_open_duration)
{
    ImGui::BeginDisabled(tree_open_duration
                         && tree_open_duration < latchlock_timeout_retry);
    if (ImGui::Button(utf8("Éjecter")))
    {
        cmd = LatchLockState::ForceOpen;
    }

    ImGui::EndDisabled();

    Vec4f color = ImGui::GetStyle().Colors[ImGuiCol_Text];
    if ((status != LatchLockState::ForceOpen)
        && (cmd == LatchLockState::ForceOpen))
    {
        color = {0.9f, 0.45f, 0.1f, 1.f};
    }
    else
    {
        cmd = LatchLockState::Unpowered;
    }

    if (tree_open_duration == 0)
    {
        ImGui::TextColored(color, utf8(">"));
    }
    else
    {
        ImGui::TextColored(color, utf8("> Éjectée"));
    }
}

struct DoorLock
{
    void
    update(Client& client)
    {
        if (ImGui::Begin(utf8("Serrures magnétiques")))
        {
            ImGui::Text(utf8("Serrures magnétiques"));
            ImGui::SameLine();
            if (client.connected)
            {
                ImGui::TextColored({0.1f, 0.9f, 0.1f, 1.f}, utf8("(Connecté)"));
            }
            else
            {
                ImGui::TextColored({0.9f, 0.1f, 0.1f, 1.f},
                                   utf8("(Déconnecté)"));
            }
            ImGui::Separator();

            ImGui::BeginDisabled(!client.connected);

            ImGui::Text(utf8("Porte Hobbit"));
            DrawLock("hobbit", command.lock_door, last_status.lock_door);
            ImGui::Separator();
            ImGui::Text(utf8("Clef dans l'arbre"));
            DrawLatchLock(command.lock_tree, last_status.lock_tree,
                          last_status.tree_open_duration);
            ImGui::Separator();
            ImGui::Text(utf8("Grotte"));
            DrawLock("cave", command.lock_cave, last_status.lock_cave);

            ImGui::EndDisabled();
        }
        ImGui::End();

        bool need_update = false;
        if (command.lock_door != last_status.lock_door)
        {
            need_update = true;
        }
        if (command.lock_cave != last_status.lock_cave)
        {
            need_update = true;
        }
        if (command.lock_tree != last_status.lock_tree)
        {
            need_update = true;
        }

        if (client.connection.socket)
        {
            if (need_update || client.heartbeatTimeout())
            {
                if (client.resendTimeout())
                {
                    client.time_command_sent = Clock::now();

                    std::vector<u8> buffer(udp_packet_size);
                    Serializer      serializer(SerializerMode::Serialize,
                                               {buffer.data(), (u32)buffer.size()});

                    command.getHeader(ClientId::Server).serialize(serializer);
                    command.serialize(serializer);

                    SendPacket(client.connection, serializer);
                }
            }
        }
    }

    DoorLockCommand command;
    DoorLockStatus  last_status;
};

template<typename T, typename R>
Str
DurationToString(std::chrono::duration<T, R> d)
{
    u64 ms  = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    Str str = fmt::format("{:02}:{:02}:{:02}.{:03}", ms / 3'600'000,
                          ms / 60000 % 60, ms / 1000 % 60, ms % 1000);
    return str;
}

int
main()
{
    SetConsoleOutputCP(CP_UTF8);
    Print("Hello.\n");

    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
    {
        PrintError("glfwInit failed.\n");
        return -1;
    }
    SCOPE_EXIT({ glfwTerminate(); });

    auto window = OpenWindow();
    SCOPE_EXIT({ glfwDestroyWindow(window); });

    InitImgui(window);
    SCOPE_EXIT({ TerminateImgui(); });

    InitServer(server);
    SCOPE_EXIT({ TerminateServer(server); });

    auto multicast_period = Milliseconds(1000);

    std::vector<Client> clients((u64)ClientId::IdMax);
    DoorLock            door_lock;

    {
        SF_FORMAT_INFO info;
        SF_INFO        sfinfo;
        int            format, major_count, subtype_count, m, s;

        memset(&sfinfo, 0, sizeof(sfinfo));
        printf("Version : %s\n\n", sf_version_string());

        sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof(int));
        sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count,
                   sizeof(int));

        sfinfo.channels = 1;
        for (m = 0; m < major_count; m++)
        {
            info.format = m;
            sf_command(NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof(info));
            printf("%s  (extension \"%s\")\n", info.name, info.extension);

            format = info.format;

            for (s = 0; s < subtype_count; s++)
            {
                info.format = s;
                sf_command(NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof(info));

                format = (format & SF_FORMAT_TYPEMASK) | info.format;

                sfinfo.format = format;
                if (sf_format_check(&sfinfo))
                    printf("   %s\n", info.name);
            };
            puts("");
        };
        puts("");
    }

    auto enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
    if (enumeration)
    {
        auto device_names = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
        Print("Devices:\n");

        auto curr = device_names;
        while (curr && *curr != '\0')
        {
            StrPtr name = curr;
            curr += name.size() + 1;
            Print("{}\n", name);
        }
    }

    auto device = alcOpenDevice(NULL);
    if (!device)
    {
        PrintError("alcOpenDevice failed\n");
        return -1;
    }

    ALCint max_source_count = 0;
    alcGetIntegerv(device, ALC_MONO_SOURCES, 1, &max_source_count);
    Print("Max mono source count: {}\n", max_source_count);

    max_source_count = 0;
    alcGetIntegerv(device, ALC_STEREO_SOURCES, 1, &max_source_count);
    Print("Max stereo source count: {}\n", max_source_count);

    auto context = alcCreateContext(device, NULL);
    if (!alcMakeContextCurrent(context))
    {
        PrintError("alcMakeContextCurrent failed\n");
    }

    SCOPE_EXIT({
        auto ctx = alcGetCurrentContext();
        if (ctx == NULL)
            return;

        device = alcGetContextsDevice(ctx);

        alcMakeContextCurrent(NULL);
        alcDestroyContext(ctx);
        alcCloseDevice(device);
    });

    const char* filename = "data/test.ogg";
    SF_INFO     sfinfo;
    auto        sndfile = sf_open(filename, SFM_READ, &sfinfo);

    if (!sndfile)
    {
        PrintError("Could not open audio in {}: {}\n", filename,
                   sf_strerror(sndfile));
    }

    SCOPE_EXIT({ sf_close(sndfile); });

    if (sfinfo.frames < 1)
    {
        PrintError("Bad sample count in {} ({})\n", filename, sfinfo.frames);
        return 0;
    }

    if (!alIsExtensionPresent("AL_EXT_FLOAT32"))
    {
        PrintError("AL_EXT_FLOAT32 extension not present\n");
        return 0;
    }
    ALint splblockalign  = 1;
    ALint byteblockalign = sfinfo.channels * 4;
    auto  format         = AL_NONE;

    if (sfinfo.channels == 1)
    {
        format = AL_FORMAT_MONO_FLOAT32;
    }
    else if (sfinfo.channels == 2)
    {
        format = AL_FORMAT_STEREO_FLOAT32;
    }

    if (sfinfo.frames / splblockalign > (sf_count_t)(INT_MAX / byteblockalign))
    {
        PrintError("Too many samples in {} ({})\n", filename, sfinfo.frames);
        return 0;
    }

    std::vector<f32> audio_buffer;
    audio_buffer.resize(sfinfo.channels * sfinfo.frames);

    auto num_frames =
        sf_readf_float(sndfile, audio_buffer.data(), sfinfo.frames);

    if (num_frames < 1)
    {
        PrintError("Failed to read samples in {} ({})\n", filename, num_frames);
        return 0;
    }

    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, audio_buffer.data(),
                 audio_buffer.size() * sizeof(f32), sfinfo.samplerate);
    SCOPE_EXIT({
        if (buffer && alIsBuffer(buffer))
            alDeleteBuffers(1, &buffer);
    });

    audio_buffer.clear();
    audio_buffer.shrink_to_fit();

    auto err = alGetError();
    if (err != AL_NO_ERROR)
    {
        PrintError("OpenAL Error: {}\n", alGetString(err));
        return 0;
    }

    ALuint source = 0;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, (ALint)buffer);

    SCOPE_EXIT({
        if (source)
            alDeleteSources(1, &source);
    });

    err = alGetError();
    if (err != AL_NO_ERROR)
    {
        PrintError("OpenAL Error: {}\n", alGetString(err));
        return 0;
    }

    alSourcePlay(source);

    auto time_start = Clock::now();
    glfwShowWindow(window);
    while (!glfwWindowShouldClose(window))
    {
        static Timepoint time_next_multicast = Clock::now();
        if (Clock::now() >= time_next_multicast)
        {
            time_next_multicast += multicast_period;

            std::vector<u8> buffer(udp_packet_size);
            Serializer      serializer(
                SerializerMode::Serialize,
                {buffer.data(), buffer.data() + buffer.size()});

            Multicast message;
            StrPtr    str = "Hey it's me, the server.";
            message.str   = {(u8*)str.data(), (u32)str.size()};
            message.getHeader().serialize(serializer);
            message.serialize(serializer);

            SendPacketMulticast(server, serializer);
        }

        glClearColor(0.2f, 0.2f, 0.2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwPollEvents();
        ImguiStartFrame();

        ImGui::ShowDemoWindow();

        while (true)
        {
            auto message = ReceiveMessage(server);

            if (message.header.client_id != ClientId::Invalid)
            {
                auto& client      = clients[(u64)message.header.client_id];
                auto& client_name = client_names[(u64)message.header.client_id];

                if (!client.connected)
                {
                    PrintSuccess("{} [{}] (new):\n",
                                 DurationToString(Clock::now() - time_start),
                                 client_name);
                    Print("{} port {}\n",
                          message.from.endpoint.address().to_string(),
                          message.from.endpoint.port());
                }
                else
                {
                    Print("{} [{}]:\n",
                          DurationToString(Clock::now() - time_start),
                          client_name);
                    if (client.connection.endpoint != message.from.endpoint)
                    {
                        PrintWarning(
                            "Client endpoint changed from {} port{},"
                            "to {} port {}\n",
                            client.connection.endpoint.address().to_string(),
                            client.connection.endpoint.port(),
                            message.from.endpoint.address().to_string(),
                            message.from.endpoint.port());
                    }
                }

                client.connected                  = true;
                client.connection                 = message.from;
                client.time_last_message_received = Clock::now();

                switch (message.header.type)
                {
                case MessageType::Multicast: {
                    PrintWarning("Server received a Multicast message\n");
                    Multicast msg;
                    msg.serialize(message.deserializer);
                }
                break;
                case MessageType::Reset: {
                    PrintWarning("Server received a reset message\n");
                    Reset msg;
                    msg.serialize(message.deserializer);
                }
                break;
                case MessageType::Log: {
                    LogMessage msg;
                    msg.serialize(message.deserializer);
                    auto str = StrPtr((char*)msg.string.start,
                                      msg.string.end - msg.string.start);
                    switch (msg.severity)
                    {
                    case LogSeverity::Info: Print("{}\n", str); break;
                    case LogSeverity::Warning: PrintWarning("{}\n", str); break;
                    case LogSeverity::Error: PrintError("{}\n", str); break;
                    case LogSeverity::Success: PrintSuccess("{}\n", str); break;
                    }
                }
                break;
                case MessageType::DoorLockCommand: {
                    PrintWarning("Server received a LockDoorCommand message\n");
                    DoorLockCommand msg;
                    msg.serialize(message.deserializer);
                }
                break;
                case MessageType::DoorLockStatus: {
                    DoorLockStatus msg;
                    msg.serialize(message.deserializer);
                    Print("   LockDoorStatus:\n");
                    Print("   Door {}\n",
                          (msg.lock_door == LockState::Locked) ? "Locked" :
                          (msg.lock_door == LockState::Open)   ? "Open" :
                                                                 "SoftLock");
                    Print("   Tree {} ({})\n",
                          (msg.lock_tree == LatchLockState::Unpowered) ?
                              "Unpowered" :
                              "ForceOpen",
                          msg.tree_open_duration);
                    Print("   Cave {}\n",
                          (msg.lock_cave == LockState::Locked) ? "Locked" :
                          (msg.lock_cave == LockState::Open)   ? "Open" :
                                                                 "SoftLock");

                    door_lock.last_status = msg;
                }
                break;

                default: {
                    PrintWarning("Message type {}\n", (u32)message.header.type);
                }
                break;
                }
            }
            else
            {
                break;
            }
        }

        for (u64 i = 0; i < clients.size(); i++)
        {
            auto& client      = clients[i];
            auto  client_name = client_names[i];
            auto  client_id   = ClientId(i);

            switch (client_id)
            {
            case ClientId::DoorLock: door_lock.update(client); break;
            default: break;
            }

            if (!client.connected)
                continue;

            auto timeout = Clock::now() - client.time_last_message_received;
            if (timeout > client_timeout_duration)
            {
                PrintWarning("[{}] timed out\n", client_name);
                client.connected = false;
                continue;
            }
        }

        ImguiEndFrame();
        glfwSwapBuffers(window);
    }
}