#pragma once

#include "ImGui/imgui.h"

class FEditorViewportToolbar
{
  public:
    static float GetDefaultHeight()
    {
        return ImGui::GetFrameHeight() + 8.0f;
    }

    static bool Begin(const char *Id, float Height = 0.0f)
    {
        if (Height <= 0.0f)
        {
            Height = GetDefaultHeight();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));

        return ImGui::BeginChild(Id, ImVec2(0.0f, Height), false,
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    }

    static void End()
    {
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
};
