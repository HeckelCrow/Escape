#include "alias.hpp"
#include "print.hpp"
#include "scope_exit.hpp"
#include "audio.hpp"
#include "console_commands.hpp"
#include "console.hpp"
#include "serial_port.hpp"
#include "server.hpp"

#define utf8(s) (const char*)u8##s

// #define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// #define NOUSER
// #include <Windows.h>
// #include <iphlpapi.h>
// #pragma comment(lib, "IPHLPAPI.lib")

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <span>
#include <chrono>
#include <atomic>
#include <random>

#include "msg/message_door_lock.hpp"
#include "msg/message_targets.hpp"
#include "msg/message_timer.hpp"
#include "msg/message_ring_dispenser.hpp"
#include "msg/wifi_config.hpp"

using Clock     = std::chrono::high_resolution_clock;
using Timepoint = std::chrono::time_point<Clock>;
using Duration  = std::chrono::microseconds;

ClientId this_client_id = ClientId::Server;

const char* client_names[] = {"Invalid", "Server", "Door Lock",     "Rings",
                              "Targets", "Timer",  "Ring Dispenser"};
static_assert(sizeof(client_names) / sizeof(client_names[0])
                  == (u64)ClientId::IdMax,
              "Client name mismatch");

constexpr Duration
Microseconds(s64 t)
{
    return std::chrono::microseconds(t);
}

constexpr Duration
Milliseconds(s64 t)
{
    return std::chrono::milliseconds(t);
}

constexpr Duration
Seconds(s64 t)
{
    return std::chrono::seconds(t);
}

constexpr Duration
Minutes(s64 t)
{
    return std::chrono::minutes(t);
}

std::random_device global_random_device;
std::mt19937       global_mt19937(global_random_device());

template<typename T>
T
Random(T min, T max)
{
    if constexpr (std::is_floating_point_v<T>)
    {
        std::uniform_real_distribution<T> distribution(min, max);
        return distribution(global_mt19937);
    }
    if constexpr (std::is_integral_v<T>)
    {
        std::uniform_int_distribution<T> distribution(min, max);
        return distribution(global_mt19937);
    }
}

template<typename T>
T
Random(T max)
{
    return Random((T)0, max);
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

template<typename T, typename R>
Str
DurationToString(std::chrono::duration<T, R> d)
{
    u64 ms  = std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
    Str str = fmt::format("{:02}:{:02}:{:02}.{:03}", ms / 3'600'000,
                          ms / 60000 % 60, ms / 1000 % 60, ms % 1000);
    return str;
}

constexpr Duration client_timeout_duration = Milliseconds(1200);
constexpr Duration heartbeat_period        = Milliseconds(700);
constexpr Duration command_resend_period   = Milliseconds(100);

struct Client
{
    bool
    timeout()
    {
        return (Clock::now() - time_last_message_received
                >= client_timeout_duration);
    }

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
        ImGui::SameLine();
        if (ImGui::Button(utf8("Annuler")))
        {
            cmd = LatchLockState::Unpowered;
        }

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
    DoorLock() {}

    void
    receiveMessage(Client& client, const DoorLockStatus& msg, bool print)
    {
        if (print)
        {
            Print("   LockDoorStatus:\n");
            Print("   Door {}\n",
                  (msg.lock_door == LockState::Locked) ? "Locked" :
                  (msg.lock_door == LockState::Open)   ? "Open" :
                                                         "SoftLock");
            Print("   Tree {} ({})\n",
                  (msg.lock_tree == LatchLockState::Unpowered) ? "Unpowered" :
                                                                 "ForceOpen",
                  msg.tree_open_duration);
            Print("   mordor {}\n",
                  (msg.lock_mordor == LockState::Locked) ? "Locked" :
                  (msg.lock_mordor == LockState::Open)   ? "Open" :
                                                           "SoftLock");
        }
        last_status = msg;
    }

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

            // ImGui::BeginDisabled(!client.connected);

            ImGui::Text(utf8("Porte Hobbit"));
            DrawLock("hobbit", command.lock_door, last_status.lock_door);
            ImGui::Separator();
            if (false) // Disable key in tree for now
            {
                ImGui::Text(utf8("Clef dans l'arbre"));
                DrawLatchLock(command.lock_tree, last_status.lock_tree,
                              last_status.tree_open_duration);
                ImGui::Separator();
            }

            ImGui::Text(utf8("Mordor"));
            DrawLock("mordor", command.lock_mordor, last_status.lock_mordor);

            // ImGui::EndDisabled();
        }
        ImGui::End();

        bool need_update = false;
        if (command.lock_door != last_status.lock_door)
        {
            need_update = true;
        }
        if (command.lock_mordor != last_status.lock_mordor)
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

                    command.getHeader().serialize(serializer);
                    command.serialize(serializer);

                    SendPacket(client.connection, serializer);
                }
            }
        }
    }

    DoorLockCommand command;
    DoorLockStatus  last_status;
};

s8
DrawOrc(u32 index, bool enabled, s8 set_hp, s8 hp)
{
    ImGui::PushID(index);
    SCOPE_EXIT({ ImGui::PopID(); });

    // ImGui::BeginDisabled(!enabled);
    // SCOPE_EXIT({ ImGui::EndDisabled(); });

    ImGui::Text(utf8("Orque %02lu"), index + 1);
    if (hp <= 0)
    {
        ImGui::SameLine();
        ImGui::Text(utf8("(mort)"));
    }

    s32 hp_s32 = hp;
    if (set_hp >= 0 && set_hp != hp)
    {
        hp_s32 = set_hp;
        ImGui::PushStyleColor(ImGuiCol_Text, {0.9f, 0.45f, 0.1f, 1.f});
    }

    if (!ImGui::SliderInt(utf8("Points de vie"), &hp_s32, 0, 5))
    {
        hp_s32 = -1;
    }
    if (set_hp >= 0 && set_hp != hp)
    {
        ImGui::PopStyleColor();
    }

    return (s8)hp_s32;
}

constexpr f32 orc_pitch_min = 0.7f;
constexpr f32 orc_pitch_max = 1.2f;

struct Targets
{
    Targets()
    {
        for (auto const& dir_entry :
             std::filesystem::directory_iterator{"data/orc/"})
        {
            if (dir_entry.is_regular_file())
            {
                Print("Loading {}\n", dir_entry.path().string());
                orcs.push_back(LoadAudioFile(dir_entry.path()));
            }
        }

        for (auto const& dir_entry :
             std::filesystem::directory_iterator{"data/orc_death/"})
        {
            if (dir_entry.is_regular_file())
            {
                Print("Loading {}\n", dir_entry.path().string());
                orc_deaths.push_back(LoadAudioFile(dir_entry.path()));
            }
        }

        for (auto const& dir_entry :
             std::filesystem::directory_iterator{"data/orc_hurt/"})
        {
            if (dir_entry.is_regular_file())
            {
                Print("Loading {}\n", dir_entry.path().string());
                orc_hurts.push_back(LoadAudioFile(dir_entry.path()));
            }
        }

        for (auto const& dir_entry :
             std::filesystem::directory_iterator{"data/orc_mad/"})
        {
            if (dir_entry.is_regular_file())
            {
                Print("Loading {}\n", dir_entry.path().string());
                orc_mads.push_back(LoadAudioFile(dir_entry.path()));
            }
        }
    }

    ~Targets()
    {
        for (auto& sound : orcs)
            DestroyAudioBuffer(sound);
        for (auto& sound : orc_deaths)
            DestroyAudioBuffer(sound);
        for (auto& sound : orc_hurts)
            DestroyAudioBuffer(sound);
        for (auto& sound : orc_mads)
            DestroyAudioBuffer(sound);
    }

    void
    receiveMessage(Client& client, const TargetsStatus& msg, bool print)
    {
        if (print)
        {
            Print("   TargetsStatus:\n");
            Print("   Enabled {}\n", msg.enabled);
            u32 i = 0;
            for (auto& h : msg.hitpoints)
            {
                Print("   - Target {} ({}): {} hp\n", i,
                      (msg.enabled & (1 << i)) ? "Enabled" : "Disabled", h);
                i++;
            }
        }
        last_status = msg;

        for (u32 i = 0; i < target_count; i++)
        {
            if (command.hitpoints[i] > last_status.hitpoints[i]
                && command.hitpoints[i] > 0)
            {
                if (last_status.hitpoints[i] <= 0)
                {
                    StopAudio(sound_playing[i]);
                    u32 rand_index   = Random(orc_deaths.size() - 1);
                    sound_playing[i] = PlayAudio(orc_deaths[rand_index]);
                    SetGain(sound_playing[i], gain_orcs_hurt / 100.f);
                    SetPitch(sound_playing[i],
                             Random(orc_pitch_min, orc_pitch_max));
                }
                else
                {
                    StopAudio(sound_playing[i]);
                    u32 rand_index   = Random(orc_deaths.size() - 1);
                    sound_playing[i] = PlayAudio(orc_deaths[rand_index]);
                    SetGain(sound_playing[i], gain_orcs_hurt / 100.f);
                    SetPitch(sound_playing[i],
                             Random(orc_pitch_min, orc_pitch_max));
                }
            }

            command.hitpoints[i] = last_status.hitpoints[i];

            if (command.set_hitpoints[i] == last_status.hitpoints[i])
            {
                // We set the command to -1 only when we received a status
                // with the right hitpoint value.
                command.set_hitpoints[i] = -1;
            }
        }

        if (msg.ask_for_ack)
        {
            command.ask_for_ack      = false;
            client.time_command_sent = Clock::now();

            std::vector<u8> buffer(udp_packet_size);
            Serializer      serializer(SerializerMode::Serialize,
                                       {buffer.data(), (u32)buffer.size()});

            command.getHeader().serialize(serializer);
            command.serialize(serializer);

            SendPacket(client.connection, serializer);
        }
    }

    void
    update(Client& client)
    {
        if (ImGui::Begin(utf8("Orques")))
        {
            ImGui::Text(utf8("Orques"));
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
            bool enable = (command.enable != 0);
            if (ImGui::Checkbox(utf8("Activer"), &enable))
            {
                if (enable)
                    command.enable = U8_MAX;
                else
                    command.enable = 0;
            }

            for (u32 i = 0; i < target_count; i++)
            {
                ImGui::Separator();
                bool enabled = command.enable & (1 << i);
                auto hp      = DrawOrc(i, enabled, command.set_hitpoints[i],
                                       last_status.hitpoints[i]);
                if (hp >= 0)
                {
                    command.set_hitpoints[i] = hp;
                }
            }
            ImGui::Separator();
            ImGui::Text(utf8("Réglages"));
            ImGui::SliderInt(utf8("Volume orques"), &gain_orcs, 0, 100);
            ImGui::SliderInt(utf8("Volume orques blessés/mort"),
                             &gain_orcs_hurt, 0, 100);

            ImGui::SliderInt(utf8("Temps mini entre cris (ms)"),
                             &min_time_between_sounds, 0, 1000);
            ImGui::SliderInt(utf8("Proba. cris (1/valeur)"), &sound_probability,
                             1, 1000);

            if (ImGui::CollapsingHeader(utf8("Boutons de sons")))
            {
                if (ImGui::Button(utf8("Orque!")))
                {
                    u32  rand_index = Random(orcs.size() - 1);
                    auto player     = PlayAudio(orcs[rand_index]);
                    SetGain(player, gain_orcs / 100.f);
                    SetPitch(player, Random(orc_pitch_min, orc_pitch_max));
                }

                if (ImGui::Button(utf8("Orque blessé!")))
                {
                    u32  rand_index = Random(orc_hurts.size() - 1);
                    auto player     = PlayAudio(orc_hurts[rand_index]);
                    SetGain(player, gain_orcs / 100.f);
                    SetPitch(player, Random(orc_pitch_min, orc_pitch_max));
                }

                if (ImGui::Button(utf8("Orque enervé!")))
                {
                    u32  rand_index = Random(orc_mads.size() - 1);
                    auto player     = PlayAudio(orc_mads[rand_index]);
                    SetGain(player, gain_orcs / 100.f);
                    SetPitch(player, Random(orc_pitch_min, orc_pitch_max));
                }

                if (ImGui::Button(utf8("Orque mort!")))
                {
                    u32  rand_index = Random(orc_deaths.size() - 1);
                    auto player     = PlayAudio(orc_deaths[rand_index]);
                    SetGain(player, gain_orcs / 100.f);
                    SetPitch(player, Random(orc_pitch_min, orc_pitch_max));
                }
            }
        }
        ImGui::End();
        if (Clock::now()
            > time_last_sound + Milliseconds(min_time_between_sounds))
        {
            for (u32 i = 0; i < target_count; i++)
            {
                bool enabled = command.enable & (1 << i);
                if (command.hitpoints[i] > 0 && enabled)
                {
                    if (!IsPlaying(sound_playing[i]))
                    {
                        command.talk = command.talk & ~(1 << i);
                        if (Random(1.f) < 1.f / sound_probability)
                        {
                            time_last_sound  = Clock::now();
                            u32 rand_index   = Random(orcs.size() - 1);
                            sound_playing[i] = PlayAudio(orcs[rand_index]);
                            SetGain(sound_playing[i], gain_orcs / 100.f);
                            SetPitch(sound_playing[i],
                                     Random(orc_pitch_min, orc_pitch_max));

                            command.talk = command.talk | (1 << i);
                        }
                    }
                }
            }
        }

        bool need_update = false;
        if (command.enable != last_status.enabled)
        {
            need_update = true;
        }
        if (command.talk != last_status.talk)
        {
            need_update = true;
        }
        for (u32 i = 0; i < target_count; i++)
        {
            if (command.set_hitpoints[i] >= 0
                && command.set_hitpoints[i] != last_status.hitpoints[i])
            {
                need_update = true;
            }
        }

        if (client.connection.socket)
        {
            if (need_update || client.heartbeatTimeout())
            {
                if (client.resendTimeout())
                {
                    command.ask_for_ack      = true;
                    client.time_command_sent = Clock::now();

                    std::vector<u8> buffer(udp_packet_size);
                    Serializer      serializer(SerializerMode::Serialize,
                                               {buffer.data(), (u32)buffer.size()});

                    command.getHeader().serialize(serializer);
                    command.serialize(serializer);

                    SendPacket(client.connection, serializer);
                }
            }
        }
    }

    TargetsCommand command;
    TargetsStatus  last_status;

    AudioPlaying sound_playing[target_count];
    Timepoint    time_last_sound;

    std::vector<AudioBuffer> orcs;
    std::vector<AudioBuffer> orc_deaths;
    std::vector<AudioBuffer> orc_hurts;
    std::vector<AudioBuffer> orc_mads;

    s32 gain_orcs      = 70;
    s32 gain_orcs_hurt = 100;

    s32 min_time_between_sounds = 700;
    s32 sound_probability       = 200;
};

// struct Timer
//{
//     Timer()
//     {
//         last_measure   = Clock::now();
//         command.paused = true;
//     }
//
//     void
//     receiveMessage(Client& client, const TimerStatus& msg, bool print)
//     {
//         if (print)
//         {
//             Print("   TimerStatus:\n");
//             Print("   Time left {:02}:{:02}\n", msg.time_left / 1000 / 60,
//                   msg.time_left / 1000 % 60);
//         }
//         last_status = msg;
//     }
//
//     void
//     update(Client& client)
//     {
//         auto now     = Clock::now();
//         auto elapsed = std::chrono::duration_cast<Duration>(now -
//         last_measure); if (!command.paused && !editing)
//         {
//             time_left -= elapsed;
//         }
//         command.time_left = (s32)DivideAndRoundDown(time_left.count(),
//         1'000); last_measure      = now;
//
//         if (ImGui::Begin(utf8("Chrono")))
//         {
//             ImGui::Text(utf8("Chrono"));
//             ImGui::SameLine();
//             if (client.connected)
//             {
//                 ImGui::TextColored({0.1f, 0.9f, 0.1f, 1.f},
//                 utf8("(Connecté)"));
//             }
//             else
//             {
//                 ImGui::TextColored({0.9f, 0.1f, 0.1f, 1.f},
//                                    utf8("(Déconnecté)"));
//             }
//             s32  minutes          = command.time_left / 1000 / 60;
//             s32  seconds          = command.time_left / 1000 % 60;
//             bool update_time_left = false;
//             editing               = false;
//             auto flags            = ImGuiInputTextFlags_AutoSelectAll;
//             ImGui::SetNextItemWidth(100);
//             if (ImGui::InputInt("##Minutes", &minutes, 1, 100, flags))
//             {
//                 update_time_left = true;
//             }
//             if (ImGui::IsItemActive())
//             {
//                 editing = true;
//             }
//             ImGui::SameLine();
//             ImGui::Text(":");
//             ImGui::SameLine();
//             ImGui::SetNextItemWidth(100);
//             if (ImGui::InputInt("##Seconds", &seconds, 1, 100, flags))
//             {
//                 update_time_left = true;
//             }
//             if (ImGui::IsItemActive())
//             {
//                 editing = true;
//             }
//             if (update_time_left)
//             {
//                 time_left = Seconds((s64)minutes * 60 + (s64)seconds);
//                 time_left += Milliseconds(999);
//             }
//
//             ImGui::BeginDisabled(!command.paused);
//             if (ImGui::Button(utf8("Go!")))
//             {
//                 command.paused = false;
//             }
//             ImGui::EndDisabled();
//             ImGui::SameLine();
//             ImGui::BeginDisabled(command.paused);
//             if (ImGui::Button(utf8("Pause")))
//             {
//                 command.paused = true;
//             }
//             ImGui::EndDisabled();
//         }
//         ImGui::End();
//
//         bool need_update = false;
//         if (std::abs(command.time_left - last_status.time_left) > 400)
//         {
//             need_update = true;
//         }
//         if (command.paused != last_status.paused)
//         {
//             need_update = true;
//         }
//
//         if (client.connection.socket)
//         {
//             if (need_update || client.heartbeatTimeout())
//             {
//                 if (client.resendTimeout())
//                 {
//                     client.time_command_sent = Clock::now();
//
//                     std::vector<u8> buffer(udp_packet_size);
//                     Serializer      serializer(SerializerMode::Serialize,
//                                                {buffer.data(),
//                                                (u32)buffer.size()});
//
//                     command.getHeader().serialize(serializer);
//                     command.serialize(serializer);
//
//                     SendPacket(client.connection, serializer);
//                 }
//             }
//         }
//     }
//
//     Timepoint last_measure;
//     Duration  time_left = Seconds(60 * 60);
//     bool      editing   = false;
//
//     TimerCommand command;
//     TimerStatus  last_status;
// };

void
DrawRing(Vec2f pos, Vec2f size, bool detected)
{
    constexpr auto col_on  = ImColor(ImVec4(0.3f, 1.f, 0.3f, 1.0f));
    constexpr auto col_off = ImColor(ImVec4(1.0f, 0.2f, 0.1f, 1.0f));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircle(pos + size * 0.5f, size.x * 0.5f,
                         detected ? col_on : col_off, 0, 4.f);
}

struct RingDispenser
{
    RingDispenser() {}

    void
    receiveMessage(Client& client, const RingDispenserStatus& msg, bool print)
    {
        if (print)
        {
            Print("   RingDispenser:\n");
            Print("   State: {}\n",
                  (msg.state == RingDispenserState::DetectRings) ?
                      "DetectRings" :
                  (msg.state == RingDispenserState::ForceDeactivate) ? "Off" :
                                                                       "On");
        }
        last_status            = msg;
        command.rings_detected = msg.rings_detected;

        if (msg.ask_for_ack)
        {
            command.ask_for_ack      = false;
            client.time_command_sent = Clock::now();

            std::vector<u8> buffer(udp_packet_size);
            Serializer      serializer(SerializerMode::Serialize,
                                       {buffer.data(), (u32)buffer.size()});

            command.getHeader().serialize(serializer);
            command.serialize(serializer);

            SendPacket(client.connection, serializer);
        }
    }

    void
    update(Client& client)
    {
        if (ImGui::Begin(utf8("Anneau Unique")))
        {
            ImGui::Text(utf8("Anneau Unique"));
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

            if (SelectableButton(utf8("Détecter les anneaux"),
                                 command.state
                                     == RingDispenserState::DetectRings))
            {
                command.state = RingDispenserState::DetectRings;
            }
            if (SelectableButton(utf8("Libérer"),
                                 command.state
                                     == RingDispenserState::ForceActivate))
            {
                command.state = RingDispenserState::ForceActivate;
            }
            ImGui::SameLine();
            if (SelectableButton(utf8("Annuler"),
                                 command.state
                                     == RingDispenserState::ForceDeactivate))
            {
                command.state = RingDispenserState::ForceDeactivate;
            }

            Vec4f color = ImGui::GetStyle().Colors[ImGuiCol_Text];
            if (last_status.state != command.state)
            {
                color = {0.9f, 0.45f, 0.1f, 1.f};
            }

            if (last_status.state == RingDispenserState::DetectRings)
            {
                ImGui::TextColored(color, utf8("> Détecter les anneaux"));
            }
            else if (last_status.state == RingDispenserState::ForceActivate)
            {
                ImGui::TextColored(color, utf8("> Libérer"));
            }
            else if (last_status.state == RingDispenserState::ForceDeactivate)
            {
                ImGui::TextColored(color, utf8("> Annuler"));
            }
            ImGui::Separator();

            const Vec2f p =
                Vec2f(ImGui::GetCursorScreenPos()) + Vec2f(4.f, 4.f);
            Vec2f pos  = p;
            Vec2f size = Vec2f(36.f);

            // Elven kings
            pos.x      = p.x + size.x * 1.5f;
            u32 ring_i = 0;
            for (u32 i = 0; i < 3; i++)
            {
                DrawRing(pos, size, last_status.rings_detected & (1 << ring_i));
                pos.x += size.x * 1.5f;
                ring_i++;
            }

            // Dwarf lords
            pos.x = p.x + size.x * 0.75f;
            pos.y += size.y * 1.5f;
            for (u32 i = 0; i < 4; i++)
            {
                DrawRing(pos, size, last_status.rings_detected & (1 << ring_i));
                pos.x += size.x * 1.5f;
                ring_i++;
            }
            pos.x = p.x + size.x * 1.5f;
            pos.y += size.y;
            for (u32 i = 0; i < 3; i++)
            {
                DrawRing(pos, size, last_status.rings_detected & (1 << ring_i));
                pos.x += size.x * 1.5f;
                ring_i++;
            }

            // Mortal Men
            pos.x = p.x + size.x * 0.75f;
            pos.y += size.y * 1.5;
            for (u32 i = 0; i < 4; i++)
            {
                DrawRing(pos, size, last_status.rings_detected & (1 << ring_i));
                pos.x += size.x * 1.5f;
                ring_i++;
            }
            pos.x = p.x;
            pos.y += size.y;
            for (u32 i = 0; i < 5; i++)
            {
                DrawRing(pos, size, last_status.rings_detected & (1 << ring_i));
                pos.x += size.x * 1.5f;
                ring_i++;
            }
        }
        ImGui::End();

        bool need_update = false;
        if (last_status.state != command.state)
        {
            need_update = true;
        }

        if (client.connection.socket)
        {
            if (need_update || client.heartbeatTimeout())
            {
                if (client.resendTimeout())
                {
                    command.ask_for_ack      = true;
                    client.time_command_sent = Clock::now();

                    std::vector<u8> buffer(udp_packet_size);
                    Serializer      serializer(SerializerMode::Serialize,
                                               {buffer.data(), (u32)buffer.size()});

                    command.getHeader().serialize(serializer);
                    command.serialize(serializer);

                    SendPacket(client.connection, serializer);
                }
            }
        }
    }

    RingDispenserCommand command;
    RingDispenserStatus  last_status;
};

s64
DivideAndRoundDown(s64 numerator, s64 denominator)
{
    if (numerator < 0 && numerator % denominator != 0)
    {
        return numerator / denominator - 1;
    }
    return numerator / denominator;
}

struct Timer
{
    Timer()
    {
        last_measure = Clock::now();
        paused       = true;

        for (auto const& dir_entry :
             std::filesystem::directory_iterator{"data/timer/"})
        {
            if (dir_entry.is_regular_file())
            {
                Print("Loading {}\n", dir_entry.path().string());
                sounds.push_back(LoadAudioFile(dir_entry.path()));
            }
        }
    }
    ~Timer()
    {
        for (auto& sound : sounds)
            DestroyAudioBuffer(sound);
    }

    Timepoint last_measure;
    Duration  time = Minutes(0);

    bool paused = false;

    s32                      sound_gain      = 70;
    bool                     play_sound_auto = true;
    u32                      sound_selected  = 0;
    std::vector<AudioBuffer> sounds;
    AudioPlaying             playing;
};

void
DrawTimer(Timer& timer)
{
    bool editing = false;
    if (ImGui::Begin(utf8("Chrono")))
    {
        ImGui::Text(utf8("Chrono"));
        auto millis      = (s32)DivideAndRoundDown(timer.time.count(), 1'000);
        s32  minutes     = millis / 1000 / 60;
        s32  seconds     = millis / 1000 % 60;
        bool update_time = false;
        auto flags       = ImGuiInputTextFlags_AutoSelectAll;
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("##Minutes", &minutes, 1, 100, flags))
        {
            update_time = true;
        }
        if (ImGui::IsItemActive())
        {
            editing = true;
        }
        ImGui::SameLine();
        ImGui::Text(":");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("##Seconds", &seconds, 1, 100, flags))
        {
            update_time = true;
        }
        if (ImGui::IsItemActive())
        {
            editing = true;
        }
        if (update_time)
        {
            timer.time = Seconds((s64)minutes * 60 + (s64)seconds);
        }

        ImGui::BeginDisabled(!timer.paused);
        if (ImGui::Button(utf8("Go!")))
        {
            timer.paused = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(timer.paused);
        if (ImGui::Button(utf8("Pause")))
        {
            timer.paused = true;
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::Text(utf8("Rappel toutes les 15 minutes"));
        ImGui::Checkbox(utf8("Jouer automatiquement"), &timer.play_sound_auto);
        if (ImGui::Button(utf8("Jouer manuellement")))
        {
            timer.playing = PlayAudio(timer.sounds[timer.sound_selected]);
            SetGain(timer.playing, timer.sound_gain / 100.f);
        }

        auto& sound_selected = timer.sounds[timer.sound_selected];
        if (ImGui::BeginCombo(utf8("Son"),
                              sound_selected.path.filename().string().c_str()))
        {
            u32 i = 0;
            for (auto& sound : timer.sounds)
            {
                const bool is_selected = (i == timer.sound_selected);
                if (ImGui::Selectable(sound.path.filename().string().c_str(),
                                      is_selected))
                {
                    timer.sound_selected = i;
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();

                i++;
            }
            ImGui::EndCombo();
        }

        if (ImGui::SliderInt(utf8("Volume"), &timer.sound_gain, 0, 100))
        {
            SetGain(timer.playing, timer.sound_gain / 100.f);
        }
    }
    ImGui::End();

    auto now = Clock::now();
    auto elapsed =
        std::chrono::duration_cast<Duration>(now - timer.last_measure);
    if (!timer.paused && !editing)
    {
        auto prev = timer.time;
        timer.time += elapsed;

        constexpr Duration play_sound_times[] = {Minutes(15), Minutes(30),
                                                 Minutes(45)};

        for (const auto& time : play_sound_times)
        {
            if (prev <= time && timer.time > time)
            {
                timer.playing = PlayAudio(timer.sounds[timer.sound_selected]);
                SetGain(timer.playing, timer.sound_gain / 100.f);
            }
        }
    }
    timer.last_measure = now;
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
                if (ImGui::Button(music.path.filename().string().c_str()))
                {
                    prev_music_playing = music_playing;
                    music_playing      = PlayAudio(music);
                    SetGain(music_playing, gain_music / 100.f);
                    StopAudio(prev_music_playing);
                }
            }
            if (music_playing.source_index != -1)
            {
                if (ImGui::Button("Stop"))
                {
                    StopAudio(music_playing);
                }
            }
        }
        ImGui::End();

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