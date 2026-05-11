#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

void FSkeletalMeshEditorToolbar::RenderViewportToolbar(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    ImGui::BeginDisabled(Mesh == nullptr);

    ImGui::AlignTextToFramePadding();
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

    ImGui::EndDisabled();
}
