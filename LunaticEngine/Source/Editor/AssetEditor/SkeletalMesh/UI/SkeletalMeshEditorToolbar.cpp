#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

void FSkeletalMeshEditorToolbar::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    ImGui::TextUnformatted("Skeletal Mesh Viewer");
    ImGui::SameLine();
    ImGui::TextDisabled("| 김연하 담당: Viewer / Preview / Details UI");

    ImGui::Separator();

    ImGui::BeginDisabled(Mesh == nullptr);

    if (ImGui::Button("Frame Mesh"))
    {
        State.bFramePreviewRequested = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Preview camera가 연결되면 mesh bounds 기준으로 카메라를 맞춘다.");
    }

    ImGui::SameLine();
    ImGui::Checkbox("Stats Overlay", &State.bShowMeshStatsOverlay);

    ImGui::SameLine();
    ImGui::TextUnformatted("LOD");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(72.0f);
    if (ImGui::InputInt("##SkeletalMeshLOD", &State.CurrentLODIndex, 1, 1))
    {
        if (State.CurrentLODIndex < 0)
        {
            State.CurrentLODIndex = 0;
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("현재는 LOD 목록 accessor가 없어 수동 index만 보관한다.\nFBX/Asset Pipeline에서 LOD API가 확정되면 combo box로 교체한다.");
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
    ImGui::Checkbox("Show Bones", &State.bShowBones);

    ImGui::SameLine();
    ImGui::Checkbox("Grid", &State.bShowGrid);

    ImGui::SameLine();
    ImGui::Checkbox("Pose Edit Mode", &State.bEnablePoseEditMode);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("김형도 담당 영역 Placeholder: Bone hierarchy / pose edit / bone gizmo 연결 예정");
    }

    ImGui::EndDisabled();
}
