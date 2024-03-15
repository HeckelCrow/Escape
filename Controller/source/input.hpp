#pragma once
#include <imgui.h>

bool
IsClicked(ImGuiKey key)
{
    return ImGui::IsKeyPressed(key, false);
}

bool
IsPressed(ImGuiKey key)
{
    return ImGui::IsKeyDown(key);
}

bool
IsClicked(ImGuiMouseButton button)
{
    return ImGui::IsMouseClicked(button, false);
}
