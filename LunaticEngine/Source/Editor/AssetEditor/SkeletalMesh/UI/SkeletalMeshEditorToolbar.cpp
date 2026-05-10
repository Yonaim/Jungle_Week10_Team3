#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

void FSkeletalMeshEditorToolbar::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    ImGui::BeginDisabled(Mesh == nullptr);

    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();

    if (ImGui::RadioButton("Reference Pose", State.PreviewMode == ESkeletalMeshPreviewMode::ReferencePose))
    {
        State.PreviewMode = ESkeletalMeshPreviewMode::ReferencePose;
    }

    ImGui::SameLine();
    if (ImGui::RadioButton("Skinned Pose", State.PreviewMode == ESkeletalMeshPreviewMode::SkinnedPose))
    {
        State.PreviewMode = ESkeletalMeshPreviewMode::SkinnedPose;
    }

    ImGui::SameLine();
    {
        const ImVec2 Cursor = ImGui::GetCursorScreenPos();
        const float Height = ImGui::GetFrameHeight();
        ImGui::Dummy(ImVec2(1.0f, Height));
        ImGui::GetWindowDrawList()->AddLine(ImVec2(Cursor.x, Cursor.y + 2.0f), ImVec2(Cursor.x, Cursor.y + Height - 2.0f),
                                            ImGui::GetColorU32(ImGuiCol_Border));
    }
    ImGui::SameLine();

    ImGui::Checkbox("Show Bones", &State.bShowBones);

    ImGui::SameLine();
    ImGui::Checkbox("Pose Edit Mode", &State.bEnablePoseEditMode);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Placeholder for Skeleton / Pose / Bone Gizmo 담당 영역");
    }

    ImGui::EndDisabled();
}
