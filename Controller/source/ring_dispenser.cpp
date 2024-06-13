#include "ring_dispenser.hpp"
#include "print.hpp"
#include "server.hpp"

#include <imgui.h>

bool SelectableButton(const char* name, bool selected);

void
DrawRing(Vec2f pos, Vec2f size, bool detected, bool enabled)
{
    constexpr auto col_on           = ImColor(ImVec4(0.3f, 1.f, 0.3f, 1.0f));
    constexpr auto col_off          = ImColor(ImVec4(1.0f, 0.2f, 0.1f, 1.0f));
    constexpr auto col_disabled = ImColor(ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

    auto color = col_disabled;
    if (enabled)
    {
        if (detected)
            color = col_on;
        else
            color = col_off;
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircle(pos + size * 0.5f, size.x * 0.5f, color, 0, 4.f);
}

void
RingDispenser::receiveMessage(Client& client, const RingDispenserStatus& msg,
                              bool print)
{
    if (print)
    {
        Print("   RingDispenser:\n");
        Print("   State: {}\n",
              (msg.state == RingDispenserState::DetectRings) ? "DetectRings" :
              (msg.state == RingDispenserState::ForceDeactivate) ? "Off" :
                                                                   "On");
        Print("   Rings:\n");
        Str rings_str;
        for (u32 i = 0; i < 19; i++)
        {
            if (msg.rings_detected & (1 << i))
            {
                rings_str += "() ";
            }
            else
            {
                rings_str += "'' ";
            }
        }
        Print("   {}\n", rings_str);
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
RingDispenser::update(Client& client)
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
            ImGui::TextColored({0.9f, 0.1f, 0.1f, 1.f}, utf8("(Déconnecté)"));
        }

        if (SelectableButton(utf8("Détecter les anneaux"),
                             command.state == RingDispenserState::DetectRings))
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
        else
        {
            ImGui::TextColored(color, utf8("> Erreur"));
        }
        ImGui::Separator();

        // Draw all the rings
        const Vec2f p    = Vec2f(ImGui::GetCursorScreenPos()) + Vec2f(4.f, 4.f);
        Vec2f       pos  = p;
        Vec2f       size = Vec2f(36.f);

        // Elven kings
        pos.x      = p.x + size.x * 1.5f;
        u32 ring_i = 0;
        for (u32 i = 0; i < 3; i++)
        {
            DrawRing(pos, size, last_status.rings_detected & (1 << ring_i),
                     client.connected);
            pos.x += size.x * 1.5f;
            ring_i++;
        }

        // Dwarf lords
        pos.x = p.x + size.x * 0.75f;
        pos.y += size.y * 1.5f;
        for (u32 i = 0; i < 4; i++)
        {
            DrawRing(pos, size, last_status.rings_detected & (1 << ring_i),
                     client.connected);
            pos.x += size.x * 1.5f;
            ring_i++;
        }
        pos.x = p.x + size.x * 1.5f;
        pos.y += size.y;
        for (u32 i = 0; i < 3; i++)
        {
            DrawRing(pos, size, last_status.rings_detected & (1 << ring_i),
                     client.connected);
            pos.x += size.x * 1.5f;
            ring_i++;
        }

        // Mortal Men
        pos.x = p.x + size.x * 0.75f;
        pos.y += size.y * 1.5;
        for (u32 i = 0; i < 4; i++)
        {
            DrawRing(pos, size, last_status.rings_detected & (1 << ring_i),
                     client.connected);
            pos.x += size.x * 1.5f;
            ring_i++;
        }
        pos.x = p.x;
        pos.y += size.y;
        for (u32 i = 0; i < 5; i++)
        {
            DrawRing(pos, size, last_status.rings_detected & (1 << ring_i),
                     client.connected);
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