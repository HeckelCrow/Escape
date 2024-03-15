#include "console.hpp"
#include "input.hpp"
#include "console_commands.hpp"
#include <imgui.h>

Console  console;
Commands console_commands;

constexpr auto console_key = ImGuiKey_GraveAccent;

s64
FindFirstDifference(StrPtr a, StrPtr b)
{
    if (a == b)
        return -1;
    for (u64 i = 0; i < a.size() && i < b.size(); i++)
    {
        if (a[i] != b[i])
            return i;
    }
    return std::min(a.size(), b.size());
}

int
ConsoleEditCallback(ImGuiInputTextCallbackData* data)
{
    StrPtr command(data->Buf, data->CursorPos);

    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion: {
        auto matches = console_commands.complete(command);
        if (matches.size())
        {
            StrPtr common = matches[0].first;
            for (s32 i = 1; i < matches.size(); i++)
            {
                auto diff = FindFirstDifference(common, matches[i].first);
                if (diff >= 0)
                {
                    common.remove_suffix(common.size() - diff);
                }
            }
            Str res(common);
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, res.data());
        }
    }
    break;
    case ImGuiInputTextFlags_CallbackHistory: {
        if (console.history_index == 0)
        {
            console.buffered_command = command;
        }

        auto prev          = console.history_index;
        bool clear_command = false;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (console.history_index < console.history.size())
                console.history_index++;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (console.history_index > 0)
            {
                console.history_index--;
            }
            else
            {
                clear_command = true;
            }
        }
        if (clear_command)
        {
            data->DeleteChars(0, data->BufTextLen);
        }
        else if (prev != console.history_index)
        {
            if (prev == 0)
                console.buffered_command = command;

            data->DeleteChars(0, data->BufTextLen);
            if (console.history_index == 0)
            {
                data->InsertChars(0, console.buffered_command.data());
            }
            else
            {
                data->InsertChars(
                    0,
                    console
                        .history[console.history.size() - console.history_index]
                        .data());
            }
        }
    }
    break;
    case ImGuiInputTextFlags_CallbackEdit: {
        console.matching_commands = console_commands.complete(command);
        console.history_index     = 0;
    }
    break;
    }
    return 0;
}

void
DrawConsole()
{
    bool grab_focus       = false;
    bool console_was_open = console.open;

    if (IsClicked(console_key))
    {
        console.open = true;
        grab_focus   = true;
    }

    if (!console.open)
        return;

    ImGui::Begin("Console", &console.open);

    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve =
        /* ImGui::GetStyle().ItemSpacing.y + */ ImGui::
            GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(4, 1)); // Tighten spacing

    auto current_type = ConsoleMessageType::Info;
    for (auto& message : console.messages)
    {
        if (current_type != message.type)
        {
            if (current_type != ConsoleMessageType::Info)
                ImGui::PopStyleColor();

            if (message.type != ConsoleMessageType::Info)
            {
                u32 color = 0;
                switch (message.type)
                {
                case ConsoleMessageType::Error:
                    color = IM_COL32(250, 50, 50, 255);
                    break;
                case ConsoleMessageType::Warning:
                    color = IM_COL32(250, 200, 50, 255);
                    break;
                case ConsoleMessageType::Success:
                    color = IM_COL32(50, 250, 50, 255);
                    break;
                }
                ImGui::PushStyleColor(ImGuiCol_Text, color);
            }
            current_type = message.type;
        }
        // ImGui::TextUnformatted(message.data());
        ImGui::TextWrapped(message.text.data());
    }
    if (current_type != ConsoleMessageType::Info)
        ImGui::PopStyleColor();

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGuiInputTextFlags input_text_flags =
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackEdit
        | ImGuiInputTextFlags_CallbackCompletion
        | ImGuiInputTextFlags_CallbackHistory;

    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##ConsoleInput", console.input_buffer.data(),
                         console.input_buffer.size(), input_text_flags,
                         &ConsoleEditCallback, nullptr))
    {
        Str command = console.input_buffer.data();

        Print("> {}\n", command);

        console.input_buffer[0] = '\0';
        console.matching_commands.clear();
        grab_focus = true;

        if (command.size())
        {
            auto err = console_commands.execute(command);
            if (err == ConsoleError::Success)
            {
                // success
            }
            else if (err == ConsoleError::UnknownCommand)
            {
                Print("Error: Cannot find command \"{}\"", command);
            }
            else if (err == ConsoleError::InvalidArguments)
            {
                Print("Error: Invalid arguments \"{}\"", command);
            }

            console.history.push_back(command);
            console.history_index = 0;
        }
    }
    ImGui::PopItemWidth();

    if (IsClicked(console_key) && console_was_open)
    {
        if (ImGui::IsItemFocused())
        {
            // We close the console is the key was pressed while the console was
            // in focus
            console.open = false;
        }
        else
        {
            grab_focus = true;
        }
    }
    if (ImGui::IsWindowAppearing() || grab_focus)
    {
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
    }

    if (console.matching_commands.size())
    {
        constexpr f32 tooltip_margin = 5.f;
        static f32    tooltip_height = 0.f;

        ImGui::SetNextWindowPos(ImVec2(
            ImGui::GetItemRectMin().x,
            ImGui::GetItemRectMin().y - (tooltip_height + tooltip_margin)));
        ImGui::SetNextWindowSize({ImGui::GetItemRectSize().x, 0});

        ImGui::PushStyleColor(ImGuiCol_PopupBg,
                              ImVec4(0.1f, 0.1f, 0.1f, 0.75f));

        ImGui::BeginTooltip();
        tooltip_height = ImGui::GetWindowHeight();
        for (const auto& match : console.matching_commands)
        {
            ImGui::TextUnformatted(match.first.data());
            ImGui::SameLine();
            ImGui::TextDisabled(match.second.data());
        }
        ImGui::EndTooltip();

        ImGui::PopStyleColor();
    }

    ImGui::End();
}
