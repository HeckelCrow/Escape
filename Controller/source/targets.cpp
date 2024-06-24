#include "targets.hpp"
#include "print.hpp"
#include "scope_exit.hpp"
#include "random.hpp"
#include "server.hpp"

#include <imgui.h>

bool SelectableButton(const char* name, bool selected);

Targets::Targets()
{
    // Loading all sound files
    for (auto const& dir_entry :
         std::filesystem::directory_iterator{"data/orc/"})
    {
        if (dir_entry.is_regular_file())
        {
            Print("Loading {}\n", dir_entry.path().string());
            auto audio_buff = LoadAudioFile(dir_entry.path());
            if (audio_buff.al_buffer != 0)
                orcs.push_back(audio_buff);
        }
    }

    for (auto const& dir_entry :
         std::filesystem::directory_iterator{"data/orc_death/"})
    {
        if (dir_entry.is_regular_file())
        {
            Print("Loading {}\n", dir_entry.path().string());
            auto audio_buff = LoadAudioFile(dir_entry.path());
            if (audio_buff.al_buffer != 0)
                orc_deaths.push_back(audio_buff);
        }
    }

    for (auto const& dir_entry :
         std::filesystem::directory_iterator{"data/orc_hurt/"})
    {
        if (dir_entry.is_regular_file())
        {
            Print("Loading {}\n", dir_entry.path().string());
            auto audio_buff = LoadAudioFile(dir_entry.path());
            if (audio_buff.al_buffer != 0)
                orc_hurts.push_back(audio_buff);
        }
    }

    for (auto const& dir_entry :
         std::filesystem::directory_iterator{"data/orc_mad/"})
    {
        if (dir_entry.is_regular_file())
        {
            Print("Loading {}\n", dir_entry.path().string());
            auto audio_buff = LoadAudioFile(dir_entry.path());
            if (audio_buff.al_buffer != 0)
                orc_mads.push_back(audio_buff);
        }
    }
}

Targets::~Targets()
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
Targets::receiveMessage(Client& client, const TargetsStatus& msg, bool print)
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
            && command.hitpoints[i] > 0 && gain_global > 0)
        {
            if (last_status.hitpoints[i] <= 0)
            {
                StopAudio(sound_playing[i]);
                u32 rand_index   = Random(orc_deaths.size() - 1);
                sound_playing[i] = PlayAudio(orc_deaths[rand_index]);
                SetGain(sound_playing[i],
                        gain_orcs_hurt / 100.f * gain_global / 100.f);
                SetPitch(sound_playing[i],
                         Random(orc_pitch_min, orc_pitch_max));
            }
            else
            {
                StopAudio(sound_playing[i]);
                u32 rand_index   = Random(orc_hurts.size() - 1);
                sound_playing[i] = PlayAudio(orc_hurts[rand_index]);
                SetGain(sound_playing[i],
                        gain_orcs_hurt / 100.f * gain_global / 100.f);
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

s8
DrawOrc(u32 index, bool enabled, s8 set_hp, s8 hp)
{
    ImGui::PushID(index);
    SCOPE_EXIT({ ImGui::PopID(); });

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

void
Targets::update(Client& client)
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
            ImGui::TextColored({0.9f, 0.1f, 0.1f, 1.f}, utf8("(Déconnecté)"));
        }

        bool enable = (command.enable != 0);
        if (ImGui::Checkbox(utf8("Activer la detection"), &enable))
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

        ImGui::Text(utf8("Porte Mordor"));
        if (SelectableButton(utf8("Ouvrir à la mort des orques"),
                             command.door_state
                                 == TargetsDoorState::OpenWhenTargetsAreDead))
        {
            command.door_state = TargetsDoorState::OpenWhenTargetsAreDead;
        }
        ImGui::SameLine();
        if (SelectableButton(utf8("Ouvrir"),
                             command.door_state == TargetsDoorState::Open))
        {
            command.door_state = TargetsDoorState::Open;
        }
        ImGui::SameLine();
        if (SelectableButton(utf8("Fermer"),
                             command.door_state == TargetsDoorState::Close))
        {
            command.door_state = TargetsDoorState::Close;
        }
        Vec4f color = ImGui::GetStyle().Colors[ImGuiCol_Text];
        if (last_status.door_state != command.door_state)
        {
            color = {0.9f, 0.45f, 0.1f, 1.f};
        }

        if (last_status.door_state == TargetsDoorState::OpenWhenTargetsAreDead)
        {
            ImGui::TextColored(color, utf8("> Ouvrir à la mort des orques"));
        }
        else if (last_status.door_state == TargetsDoorState::Open)
        {
            ImGui::TextColored(color, utf8("> Ouvrir"));
        }
        else if (last_status.door_state == TargetsDoorState::Close)
        {
            ImGui::TextColored(color, utf8("> Fermer"));
        }
        else
        {
            ImGui::TextColored(color, utf8("> Erreur"));
        }
        ImGui::Separator();

        ImGui::Text(utf8("Réglages"));
        ImGui::SliderInt(utf8("Volume général"), &gain_global, 0, 100);
        ImGui::SliderInt(utf8("Volume bruits d'orque"), &gain_orcs, 0, 100);
        ImGui::SliderInt(utf8("Volume orques blessés/mort"), &gain_orcs_hurt, 0,
                         100);

        ImGui::SliderInt(utf8("Temps mini entre cris (ms)"),
                         &min_time_between_sounds, 0, 1000);
        ImGui::SliderInt(utf8("Proba. cris (1/valeur)"), &sound_probability, 1,
                         1000);

        if (ImGui::CollapsingHeader(utf8("Boutons de sons")))
        {
            if (gain_global == 0)
            {
                ImGui::TextColored({0.9f, 0.45f, 0.1f, 1.f},
                                   utf8("Les boutons sont désactivés parce que "
                                        "le volume général est à 0"));
            }
            else
            {
                if (gain_orcs == 0)
                {
                    ImGui::TextColored(
                        {0.9f, 0.45f, 0.1f, 1.f},
                        utf8("Certains boutons sont désactivés parce que "
                             "le volume de bruits d'orque est à 0"));
                }
                if (gain_orcs_hurt == 0)
                {
                    ImGui::TextColored(
                        {0.9f, 0.45f, 0.1f, 1.f},
                        utf8("Certains boutons sont désactivés parce que "
                             "le volume d'orques blessés/mort est à 0"));
                }
            }

            ImGui::BeginDisabled(gain_orcs == 0 || gain_global == 0);
            if (ImGui::Button(utf8("Orque!")))
            {
                u32  rand_index = Random(orcs.size() - 1);
                auto player     = PlayAudio(orcs[rand_index]);
                SetGain(player, gain_orcs / 100.f * gain_global / 100.f);
                SetPitch(player, Random(orc_pitch_min, orc_pitch_max));
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(gain_orcs_hurt == 0 || gain_global == 0);
            if (ImGui::Button(utf8("Orque blessé!")))
            {
                u32  rand_index = Random(orc_hurts.size() - 1);
                auto player     = PlayAudio(orc_hurts[rand_index]);
                SetGain(player, gain_orcs_hurt / 100.f * gain_global / 100.f);
                SetPitch(player, Random(orc_pitch_min, orc_pitch_max));
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(gain_orcs == 0 || gain_global == 0);
            if (ImGui::Button(utf8("Orque enervé!")))
            {
                u32  rand_index = Random(orc_mads.size() - 1);
                auto player     = PlayAudio(orc_mads[rand_index]);
                SetGain(player, gain_orcs / 100.f * gain_global / 100.f);
                SetPitch(player, Random(orc_pitch_min, orc_pitch_max));
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(gain_orcs_hurt == 0 || gain_global == 0);
            if (ImGui::Button(utf8("Orque mort!")))
            {
                u32  rand_index = Random(orc_deaths.size() - 1);
                auto player     = PlayAudio(orc_deaths[rand_index]);
                SetGain(player, gain_orcs_hurt / 100.f * gain_global / 100.f);
                SetPitch(player, Random(orc_pitch_min, orc_pitch_max));
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::End();
    if (Clock::now() > time_last_sound + Milliseconds(min_time_between_sounds))
    {
        for (u32 i = 0; i < target_count; i++)
        {
            bool enabled = command.enable & (1 << i);
            if (command.hitpoints[i] > 0 && enabled && gain_global > 0)
            {
                if (!IsPlaying(sound_playing[i]))
                {
                    if (Random(1.f) < 1.f / sound_probability)
                    {
                        time_last_sound  = Clock::now();
                        u32 rand_index   = Random(orcs.size() - 1);
                        sound_playing[i] = PlayAudio(orcs[rand_index]);
                        SetGain(sound_playing[i],
                                gain_orcs / 100.f * gain_global / 100.f);
                        SetPitch(sound_playing[i],
                                 Random(orc_pitch_min, orc_pitch_max));
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
    if (command.send_sensor_data != last_status.send_sensor_data)
    {
        need_update = true;
    }
    if (command.door_state != last_status.door_state)
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