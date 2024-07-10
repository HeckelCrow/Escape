#include "alias.hpp"
#include "print.hpp"
#include "scope_exit.hpp"
#include "audio.hpp"
#include "console_commands.hpp"
#include "console.hpp"
#include "serial_port.hpp"
#include "server.hpp"
#include "settings.hpp"
#include "time.hpp"
#include "random.hpp"

#include "door_lock.hpp"
#include "targets.hpp"
#include "ring_dispenser.hpp"
#include "timer.hpp"

// #include "msg/message_timer.hpp"
#include "msg/wifi_config.hpp"

// #define WIN32_LEAN_AND_MEAN
// #define NOMINMAX
// #define NOUSER
// #include <Windows.h>
// #include <iphlpapi.h>
// #pragma comment(lib, "IPHLPAPI.lib")

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <implot.h>

#include <span>
#include <atomic>

#if IS_DEBUG == 0
#    pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

ClientId this_client_id = ClientId::Server;

const char* client_names[] = {"Invalid", "Server", "Door Lock",     "Rings",
                              "Targets", "Timer",  "Ring Dispenser"};
static_assert(sizeof(client_names) / sizeof(client_names[0])
                  == (u64)ClientId::IdMax,
              "Client name mismatch");

std::random_device global_random_device;
std::mt19937       global_mt19937(global_random_device());

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

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

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

    // if (gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress))
    //{
    //     Print("OpenGL ES {}.{}\n", GLVersion.major, GLVersion.minor);
    // }
    if (gladLoadGL())
    {
        Print("OpenGL  {}.{}\n", GLVersion.major, GLVersion.minor);
    }
    else
    {
        PrintError("gladLoadGLES2Loader failed.\n");
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
    // ImGui_ImplOpenGL3_Init("#version 330");
    ImGui_ImplOpenGL3_Init("#version 100");

    ImPlot::CreateContext();
    ImGui::GetStyle().AntiAliasedLines = true;
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

int
main(int argc, char* argv[])
{
    bool alloc_console = false;
    if (argc == 2)
    {
        if (strcmp(argv[1], "-console") == 0)
        {
            if (AllocConsole())
            {
                alloc_console = true;
                FILE* fp;
                freopen_s(&fp, "CONOUT$", "w", stdout);
                freopen_s(&fp, "CONIN$", "r", stdin);
                freopen_s(&fp, "CONOUT$", "w", stderr);
            }
        }
    }
    SCOPE_EXIT({
        if (alloc_console)
            FreeConsole();
    })
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

    Server server = {};
    InitServer(server);
    SCOPE_EXIT({ TerminateServer(server); });

    auto multicast_period = Milliseconds(1000);

    InitAudio(32);
    SCOPE_EXIT({ TerminateAudio(); });

    std::vector<Client> clients((u64)ClientId::IdMax);
    DoorLock            door_lock;
    Targets             targets;
    // Timer               timer;
    RingDispenser ring_dispenser;

    Timer timer;

    std::vector<u16> target_graphs[target_count];

    std::vector<AudioBuffer> musics;
    AudioPlaying             music_playing;
    AudioPlaying             prev_music_playing;

    for (auto const& dir_entry :
         std::filesystem::directory_iterator{"data/musics/"})
    {
        if (dir_entry.is_regular_file())
        {
            Print("Loading {}\n", dir_entry.path().string());
            musics.push_back(LoadAudioFile(dir_entry.path(), true));
        }
    }

    RegisterConsoleCommand(
        "help", {}, std::function([&]() {
            PrintSuccess("Command list:");
            for (auto& cmd : console_commands.commands)
            {
                Print(cmd.name + " " + Concatenate(cmd.argument_names, " "));
            }
        }));
    RegisterConsoleCommand("clear", {}, std::function([&]() {
                               console.messages.clear();
                               console.messages.shrink_to_fit();
                           }));

    RegisterConsoleCommand("quit", {}, std::function([&]() {
                               glfwSetWindowShouldClose(window, true);
                           }));

    RegisterConsoleCommand("sethistorysize", {"u32 max_message_count"},
                           std::function([&](u32 max_msg_count) {
                               console.message_max_count = max_msg_count;
                               PrintSuccess("console.message_max_count = {}\n",
                                            console.message_max_count);
                           }));

    bool show_messages_received = false;
    RegisterConsoleCommand(
        "showmessages", {"bool show"}, std::function([&](u8 print) {
            show_messages_received = print;
            PrintSuccess("show_messages_received = {}\n",
                         show_messages_received ? "show" : "hide");
        }));

    RegisterConsoleCommand("disconectall", {}, std::function([&]() {
                               for (u64 i = 0; i < clients.size(); i++)
                               {
                                   auto& client      = clients[i];
                                   auto  client_name = client_names[i];

                                   if (client.connected)
                                   {
                                       PrintSuccess("[{}] disconnected\n",
                                                    client_name);
                                       client.connected = false;
                                   }
                               }
                           }));
    RegisterConsoleCommand(
        "resetall", {}, std::function([&]() {
            for (u64 i = 0; i < clients.size(); i++)
            {
                auto& client      = clients[i];
                auto  client_name = client_names[i];

                if (client.connection.socket)
                {
                    client.time_command_sent = Clock::now();

                    std::vector<u8> buffer(udp_packet_size);
                    Serializer      serializer(SerializerMode::Serialize,
                                               {buffer.data(), (u32)buffer.size()});

                    Reset msg;
                    msg.getHeader().serialize(serializer);
                    msg.serialize(serializer);

                    SendPacket(client.connection, serializer);
                }

                if (client.connected)
                {
                    PrintSuccess("Reset [{}]\n", client_name);
                    client.connected = false;
                }
            }
        }));

    RegisterConsoleCommand("listserialports", {},
                           std::function([&]() { ListSerialPorts(); }));

    bool               listen_to_serial_ports = false;
    constexpr Duration serial_scan_period     = Seconds(1);
    RegisterConsoleCommand(
        "serial", {"bool listen"}, std::function([&](u8 listen) {
            listen_to_serial_ports = listen;
            PrintSuccess("listen_to_serial_ports = {}\n",
                         listen_to_serial_ports ? "true" : "false");
            if (listen)
            {
                InitSerial();
            }
            else
            {
                TerminateSerial();
            }
        }));

    RegisterConsoleCommand("showtargetsensor", {"bool show"},
                           std::function([&](u8 show) {
                               show = (show != 0);
                               if (show)
                               {
                                   PrintSuccess("Show target sensor data\n");
                               }
                               else
                               {
                                   PrintSuccess("No target sensor data\n");
                                   for (auto& graph : target_graphs)
                                   {
                                       graph.clear();
                                       graph.shrink_to_fit();
                                   }
                               }
                               targets.command.send_sensor_data = show;
                           }));

    // RegisterConsoleCommand(
    //     "setthreshold", std::vector<StrPtr>{"u8 target", "u16 value"},
    //     std::function([&](StrPtr args) {
    //         u64 i = 0;
    //         ReadAllSpaces(args, i);
    //         u8 target;
    //         if (!TryRead(args, i, target))
    //             return;
    //         ReadAllSpaces(args, i);
    //         u16 value;
    //         if (!TryRead(args, i, value))
    //             return;

    //        if (target > 0 && target <= target_count)
    //        {
    //            PrintSuccess("Target {} threshold set to {}\n", target,
    //            value); targets.command.thresholds[target - 1] = value;
    //        }
    //        else
    //        {
    //            PrintWarning("Target {} is invalid (min {}, max {})\n",
    //            target,
    //                         1, target_count);
    //        }
    //    }));

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

        DrawConsole();
        DrawTimer(timer);

        UpdateAudio();

        if (listen_to_serial_ports)
        {
            static Timepoint next_serial_scan = Clock::now();
            bool             do_scan          = false;
            if (Clock::now() >= next_serial_scan)
            {
                next_serial_scan = Clock::now() + serial_scan_period;
                do_scan          = true;
            }
            UpdateSerial(do_scan);
        }

        static bool show_demo = false;
        if (show_demo)
            ImGui::ShowDemoWindow(&show_demo);

        if (ImGui::IsKeyPressed(ImGuiKey_F1))
            show_demo = true;

        if (ImGui::Begin("Audio"))
        {
            static s32 gain_music = 50;
            if (ImGui::SliderInt(utf8("Volume musique"), &gain_music, 0, 100))
            {
                SetGain(music_playing, gain_music / 100.f);
            }

            for (auto& music : musics)
            {
                if (ImGui::Button(
                        (const char*)music.path.filename().u8string().c_str()))
                {
                    prev_music_playing = music_playing;
                    music_playing      = PlayAudio(music);
                    SetGain(music_playing, gain_music / 100.f);
                    StopAudio(prev_music_playing);
                }
            }
            if (IsPlaying(music_playing))
            {
                if (ImGui::Button(utf8("Arrêter la musique")))
                {
                    StopAudio(music_playing);
                }
            }
        }
        ImGui::End();

        if (targets.command.send_sensor_data)
        {
            if (ImGui::Begin("Targets graph"))
            {
                ImPlot::BeginPlot("Targets plot", {-1, -1});
                for (u32 i = 0; i < target_count; i++)
                {
                    if (target_graphs[i].size())
                    {
                        auto name = fmt::format("Target {}", i + 1);

                        ImPlot::PlotLine(name.c_str(), target_graphs[i].data(),
                                         target_graphs[i].size());

                        // ImPlot::TagY(targets.command.thresholds[i],
                        //              ImPlot::GetLastItemColor(), "%d", i
                        //              + 1);

                        ImPlot::SetNextLineStyle(ImPlot::GetLastItemColor());

                        name       = fmt::format("Threshold {}", i + 1);
                        f32 h_line = targets.last_status.thresholds[i];
                        ImPlot::PlotInfLines(name.c_str(), &h_line, 1,
                                             ImPlotInfLinesFlags_Horizontal);
                    }
                }
                ImPlot::EndPlot();
            }
            ImGui::End();
        }

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

                    if (server.sockets.size() > 1)
                    {
                        // We found the right socket, we keep it and close
                        // the other ones.
                        server.sockets[0] = message.from.socket;
                        server.sockets.resize(1);
                    }
                }
                else if (show_messages_received
                         || message.header.type == MessageType::Log)
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

                    door_lock.receiveMessage(client, msg,
                                             show_messages_received);
                }
                break;

                case MessageType::TargetsCommand: {
                    PrintWarning("Server received a TargetsCommand message\n");
                    TargetsCommand msg;
                    msg.serialize(message.deserializer);
                }
                break;
                case MessageType::TargetsStatus: {
                    TargetsStatus msg;
                    msg.serialize(message.deserializer);
                    targets.receiveMessage(client, msg, show_messages_received);
                }
                break;
                case MessageType::TargetsGraph: {
                    TargetsGraph msg;
                    msg.serialize(message.deserializer);
                    for (u32 i = 0; i < target_count; i++)
                    {
                        target_graphs[i].insert_range(
                            target_graphs[i].end(),
                            std::span(msg.buffer[i], msg.buffer_count[i]));
                    }
                }
                break;

                    // case MessageType::TimerCommand: {
                    //     PrintWarning("Server received a TimerCommand
                    //     message\n"); TimerCommand msg;
                    //     msg.serialize(message.deserializer);
                    // }
                    // break;
                    // case MessageType::TimerStatus: {
                    //     TimerStatus msg;
                    //     msg.serialize(message.deserializer);
                    //     timer.receiveMessage(client, msg,
                    //     show_messages_received);
                    // }
                    // break;

                case MessageType::RingDispenserCommand: {
                    PrintWarning(
                        "Server received a RingDispenserCommand message\n");
                    RingDispenserCommand msg;
                    msg.serialize(message.deserializer);
                }
                break;
                case MessageType::RingDispenserStatus: {
                    RingDispenserStatus msg;
                    msg.serialize(message.deserializer);
                    ring_dispenser.receiveMessage(client, msg,
                                                  show_messages_received);
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

        u32 clients_connected_count = 0;
        for (u64 i = 0; i < clients.size(); i++)
        {
            auto& client      = clients[i];
            auto  client_name = client_names[i];
            auto  client_id   = ClientId(i);

            switch (client_id)
            {
            case ClientId::DoorLock: door_lock.update(client); break;
            case ClientId::Targets: targets.update(client); break;
            // case ClientId::Timer: timer.update(client); break;
            case ClientId::RingDispenser: ring_dispenser.update(client); break;
            default: break;
            }

            if (!client.connected)
                continue;
            if (client.timeout())
            {
                PrintWarning("[{}] timed out\n", client_name);
                client.connected = false;
                continue;
            }
            clients_connected_count++;
        }
        if (clients_connected_count == 0)
        {
            ImGui::Begin(utf8("Initialisation"));
            auto str = fmt::format(
                fmt::runtime(utf8("Connectez vous au réseau wifi \"{}\" avec "
                                  "le mot de passe \"{}\".\n")),
                wifi_ssid, wifi_password);
            ImGui::TextWrapped(str.c_str());
            ImGui::TextWrapped(
                utf8("Si ça ne marche pas, appuyez sur réessayer."));
            if (ImGui::Button(utf8("Réessayer")))
            {
                PrintSuccess("Reset server\n");
                TerminateServer(server);
                InitServer(server);
            }
            ImGui::End();
        }

        ImguiEndFrame();
        glfwSwapBuffers(window);
    }
}