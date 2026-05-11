#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

void FSkeletalMeshEditorToolbar::RenderViewportToolbar(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    ImGui::BeginDisabled(Mesh == nullptr);

    if (ImGui::Button("Frame"))
    {
        State.bFramePreviewRequested = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Frame the current skeletal mesh in the preview camera.");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Stats", &State.bShowMeshStatsOverlay);

    ImGui::SameLine();
    ImGui::Checkbox("Bones", &State.bShowBones);

    ImGui::SameLine();
    ImGui::Checkbox("Grid", &State.bShowGrid);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    int32 PreviewMode = static_cast<int32>(State.PreviewMode);
    const char *PreviewModes[] = {"Reference Pose", "Skinned Pose"};
    if (ImGui::Combo("##SkeletalPreviewMode", &PreviewMode, PreviewModes, IM_ARRAYSIZE(PreviewModes)))
    {
        State.PreviewMode = static_cast<ESkeletalMeshPreviewMode>(PreviewMode);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("LOD");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(52.0f);
    if (ImGui::InputInt("##SkeletalMeshLOD", &State.CurrentLODIndex, 1, 1) && State.CurrentLODIndex < 0)
    {
        State.CurrentLODIndex = 0;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("The importer currently fills LOD0 only.");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Pose Edit", &State.bEnablePoseEditMode);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Reserved for future pose edit and bone gizmo work.");
    }

    ImGui::EndDisabled();
}
