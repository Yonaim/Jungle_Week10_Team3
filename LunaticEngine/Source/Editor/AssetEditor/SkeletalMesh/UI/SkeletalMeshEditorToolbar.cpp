#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

void FSkeletalMeshEditorToolbar::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    ImGui::TextUnformatted("Skeletal Mesh");
    ImGui::TextDisabled("Viewer / Preview / Details");
    ImGui::Separator();

    ImGui::BeginDisabled(Mesh == nullptr);

    if (ImGui::Button("Frame Mesh", ImVec2(-1.0f, 0.0f)))
    {
        State.bFramePreviewRequested = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Preview camera가 연결되면 mesh bounds 기준으로 카메라를 맞춘다.");
    }

    ImGui::Spacing();
    ImGui::Checkbox("Stats Overlay", &State.bShowMeshStatsOverlay);
    ImGui::Checkbox("Show Bones", &State.bShowBones);
    ImGui::Checkbox("Grid", &State.bShowGrid);

    ImGui::SeparatorText("Preview");
    if (ImGui::RadioButton("Reference Pose", State.PreviewMode == ESkeletalMeshPreviewMode::ReferencePose))
    {
        State.PreviewMode = ESkeletalMeshPreviewMode::ReferencePose;
    }
    if (ImGui::RadioButton("Skinned Pose", State.PreviewMode == ESkeletalMeshPreviewMode::SkinnedPose))
    {
        State.PreviewMode = ESkeletalMeshPreviewMode::SkinnedPose;
    }

    ImGui::SeparatorText("LOD");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputInt("##SkeletalMeshLOD", &State.CurrentLODIndex, 1, 1))
    {
        if (State.CurrentLODIndex < 0)
        {
            State.CurrentLODIndex = 0;
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("현재 SkeletalMesh Importer는 LOD0만 채운다.\n추후 LOD 배열이 생기면 combo box로 교체한다.");
    }

    ImGui::SeparatorText("Edit");
    ImGui::Checkbox("Pose Edit Mode", &State.bEnablePoseEditMode);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("김형도 담당 예정 영역: pose edit / bone gizmo 연결 지점");
    }

    ImGui::EndDisabled();
}
