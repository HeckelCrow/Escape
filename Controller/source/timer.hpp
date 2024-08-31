#pragma once
#include "alias.hpp"
#include "time.hpp"
#include "audio.hpp"

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

struct Timer
{
    Timer();
    ~Timer();

    Timepoint last_measure;
    Duration  time            = Minutes(0);
    Duration  reminder_period = Minutes(30);

    bool paused = false;

    s32                      sound_gain      = 70;
    bool                     play_sound_auto = true;
    u32                      sound_selected  = 0;
    std::vector<AudioBuffer> sounds;
    AudioPlaying             playing;
};

void DrawTimer(Timer& timer);