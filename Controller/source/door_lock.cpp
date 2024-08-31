#include "door_lock.hpp"
#include "print.hpp"
#include "scope_exit.hpp"
#include "server.hpp"

#include <imgui.h>

bool SelectableButton(const char* name, bool selected);

void
DoorLock::receiveMessage(Client& client, const DoorLockStatus& msg, bool print)
{
    if (print)
    {
        Print("   LockDoorStatus:\n");
        Print("   Door {}\n", (msg.lock_door == LockState::Locked) ? "Locked" :
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
        // ImGui::TextColored(color, utf8("> Déverrouillée"));
    }
    else if (status == LockState::SoftLock)
    {
        ImGui::TextColored(color, utf8("> Fermeture douce"));
    }
    else if (status == LockState::Locked)
    {
        ImGui::TextColored(color, utf8("> Verrouillée"));
    }
    else
    {
        ImGui::TextColored(color, utf8("> Erreur"));
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

void
DoorLock::update(Client& client)
{
    // DoorLock was supposed to control a lot of things but the plans have
    // changed. I kept the code just in case. DoorLockCommand and DoorLockStatus
    // send some useless data because of that. It was easier to hide the
    // controls instead of really removing things.

    // if (ImGui::Begin(utf8("Serrures magnétiques")))
    if (ImGui::Begin(utf8("Porte Hobbit")))
    {
        // ImGui::Text(utf8("Serrures magnétiques"));
        ImGui::Text(utf8("Porte Hobbit"));
        ImGui::SameLine();
        if (client.connected)
        {
            ImGui::TextColored({0.1f, 0.9f, 0.1f, 1.f}, utf8("(Connecté)"));
        }
        else
        {
            ImGui::TextColored({0.9f, 0.1f, 0.1f, 1.f}, utf8("(Déconnecté)"));
        }
        ImGui::Separator();

        // ImGui::Text(utf8("Porte Hobbit"));
        DrawLock("hobbit", command.lock_door, last_status.lock_door);
        ImGui::Separator();
        if (false) // Disable key in tree for now
        {
            ImGui::Text(utf8("Clef dans l'arbre"));
            DrawLatchLock(command.lock_tree, last_status.lock_tree,
                          last_status.tree_open_duration);
            ImGui::Separator();
        }
        if (false) // Mordor door is controled by something else
        {
            ImGui::Text(utf8("Mordor"));
            DrawLock("mordor", command.lock_mordor, last_status.lock_mordor);
        }
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