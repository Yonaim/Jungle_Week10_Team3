#include "LevelEditor/UI/Area/LevelViewportArea.h"

#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/Viewport/EditorViewportPanel.h"
#include "EditorEngine.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "ImGui/imgui.h"
#include "Viewport/Viewport.h"


void FLevelViewportArea::SetIndex(int32 InIndex)
{
    Index = InIndex;
    if (Index == 0)
    {
        WindowName = "Viewport";
    }
    else
    {
        WindowName = "Viewport " + std::to_string(Index + 1);
    }
}

void FLevelViewportArea::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 1.0f));
    constexpr const char *PanelIconKey = "Editor.Icon.Panel.Viewport";
    const std::string StableId = std::string("LevelViewportArea_") + std::to_string(Index);
    FPanelDesc PanelDesc;
    PanelDesc.DisplayName = WindowName.c_str();
    PanelDesc.StableId = StableId.c_str();
    PanelDesc.IconKey = PanelIconKey;
    PanelDesc.WindowFlags = ImGuiWindowFlags_None;
    PanelDesc.bApplySideInset = false;
    PanelDesc.bApplyBottomInset = false;
    const bool bIsOpen = FPanel::Begin(PanelDesc);
    if (!bIsOpen)
    {
        FPanel::End();
        ImGui::PopStyleVar(2);
        return;
    }

    if (FEditorViewportPanel::BeginTopToolbar("##ViewportToolbar"))
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.0f);
        float ButtonWidth = ImGui::CalcTextSize("Split").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - ButtonWidth);

        bool bIsSplit = EditorEngine ? EditorEngine->IsSplitViewport() : false;
        const char *ButtonLabel = bIsSplit ? "Merge" : "Split";

        if (ImGui::Button(ButtonLabel))
        {
            if (EditorEngine)
            {
                EditorEngine->ToggleViewportSplit();
            }
        }
    }
    FEditorViewportPanel::EndTopToolbar();

    if (ViewportClient)
    {
        const bool bActive = EditorEngine && EditorEngine->GetActiveViewport() == ViewportClient;
        FEditorViewportPanel::RenderViewportClient(*ViewportClient, bActive);

        if (EditorEngine && ViewportClient->IsHovered() &&
            (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
        {
            if (EditorEngine->GetActiveViewport() != ViewportClient)
            {
                EditorEngine->SetActiveViewport(ViewportClient);
            }
        }
    }

    FPanel::End();
    ImGui::PopStyleVar(2);
}
