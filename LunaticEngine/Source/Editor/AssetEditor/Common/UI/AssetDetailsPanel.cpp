#include "AssetEditor/Common/UI/AssetDetailsPanel.h"

#include "Common/UI/Style/EditorUIStyle.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Platform/Paths.h"
#include "ImGui/imgui.h"

namespace
{
const char *PreviewModeToText(ESkeletalMeshPreviewMode Mode)
{
    switch (Mode)
    {
    case ESkeletalMeshPreviewMode::ReferencePose:
        return "Reference Pose";
    case ESkeletalMeshPreviewMode::SkinnedPose:
        return "Skinned Pose";
    default:
        return "Unknown";
    }
}
} // namespace

void FAssetDetailsPanel::RenderSkeletalMesh(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath,
                                            FSkeletalMeshEditorState &State, const FPanelDesc &PanelDesc)
{
    if (!FPanel::Begin(PanelDesc))
    {
        FPanel::End();
        return;
    }

    if (!Mesh)
    {
        ImGui::TextDisabled("No SkeletalMesh asset selected.");
        FPanel::End();
        return;
    }

    RenderMeshInfo(Mesh, AssetPath, State);
    ImGui::Spacing();
    RenderLODSectionMaterialInfo(Mesh, State);
    ImGui::Spacing();
    RenderViewerActions(State);
    ImGui::Spacing();
    RenderRuntimePlaceholder();
    ImGui::Spacing();
    RenderBoneEditingPlaceholder(State);

    FPanel::End();
}

void FAssetDetailsPanel::RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath,
                                        FSkeletalMeshEditorState &State)
{
    if (!FEditorUIStyle::BeginDetailsSection("Mesh Info"))
    {
        return;
    }

    const FString FileName = AssetPath.empty() ? FString("Untitled") : FPaths::ToUtf8(AssetPath.filename().wstring());
    const FString FullPath = AssetPath.empty() ? FString("") : FPaths::ToUtf8(AssetPath.wstring());

    if (FEditorUIStyle::BeginDetailsReadOnlyTable("##AssetDetailsSkeletalMeshInfoTable"))
    {
        FEditorUIStyle::DrawReadOnlyTextRow("Asset", FileName.c_str());
        if (!FullPath.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", FullPath.c_str());
        }

        FEditorUIStyle::DrawReadOnlyIntRow("Bone Count", Mesh->GetBoneCount());
        FEditorUIStyle::DrawReadOnlyIntRow("Vertex Count", Mesh->GetVertexCount());
        FEditorUIStyle::DrawReadOnlyIntRow("Index Count", Mesh->GetIndexCount());
        FEditorUIStyle::DrawReadOnlyIntRow("Current LOD", State.CurrentLODIndex);
        FEditorUIStyle::DrawReadOnlyIntRow("Selected Bone", State.SelectedBoneIndex);
        FEditorUIStyle::DrawReadOnlyTextRow("Preview Mode", PreviewModeToText(State.PreviewMode));
        ImGui::EndTable();
    }
}

void FAssetDetailsPanel::RenderLODSectionMaterialInfo(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    (void)Mesh;

    if (!FEditorUIStyle::BeginDetailsSection("LOD / Section / Material"))
    {
        return;
    }

    ImGui::TextDisabled("김연하 담당 Viewer UI 영역");
    ImGui::BulletText("현재 선택 LOD / Section / Material Slot 상태를 보관한다.");
    ImGui::BulletText("FBX / Asset Pipeline에서 LOD, Section, Material accessor가 확정되면 실제 목록으로 교체한다.");

    ImGui::Separator();

    FEditorUIStyle::PushDetailsPropertyEditStyle();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("Current LOD", &State.CurrentLODIndex, 1, 1) && State.CurrentLODIndex < 0)
    {
        State.CurrentLODIndex = 0;
    }

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Selected Section", &State.SelectedSectionIndex, 1, 1);
    if (State.SelectedSectionIndex < -1)
    {
        State.SelectedSectionIndex = -1;
    }

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputInt("Material Slot", &State.SelectedMaterialSlotIndex, 1, 1);
    if (State.SelectedMaterialSlotIndex < -1)
    {
        State.SelectedMaterialSlotIndex = -1;
    }
    FEditorUIStyle::PopDetailsPropertyEditStyle();

    if (ImGui::BeginTable("##AssetDetailsSkeletalMeshLODPlaceholderTable", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("LOD");
        ImGui::TableSetupColumn("Section");
        ImGui::TableSetupColumn("Material");
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%d", State.CurrentLODIndex);
        ImGui::TableNextColumn();
        ImGui::Text("%s", State.SelectedSectionIndex >= 0 ? "Selected" : "Pending API");
        ImGui::TableNextColumn();
        ImGui::Text("%s", State.SelectedMaterialSlotIndex >= 0 ? "Selected" : "Pending API");

        ImGui::EndTable();
    }
}

void FAssetDetailsPanel::RenderViewerActions(FSkeletalMeshEditorState &State)
{
    if (!FEditorUIStyle::BeginDetailsSection("Viewer Actions"))
    {
        return;
    }

    FEditorUIStyle::PushHeaderButtonStyle();
    if (ImGui::Button("Frame Mesh"))
    {
        State.bFramePreviewRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Bone Selection"))
    {
        State.SelectedBoneIndex = -1;
    }
    FEditorUIStyle::PopHeaderButtonStyle();

    ImGui::Checkbox("Show Mesh Stats Overlay", &State.bShowMeshStatsOverlay);
    ImGui::Checkbox("Show Bones", &State.bShowBones);
}

void FAssetDetailsPanel::RenderRuntimePlaceholder()
{
    if (FEditorUIStyle::BeginDetailsSection("Runtime / Skinning"))
    {
        ImGui::TextDisabled("남윤지 담당 영역 Placeholder");
        ImGui::BulletText("USkinnedMeshComponent / USkeletalMeshComponent 연동 예정");
        ImGui::BulletText("Reference Pose / CPU Skinning 결과 표시 예정");
        ImGui::BulletText("Preview Viewport의 실제 렌더링 입력으로 연결 예정");
    }
}

void FAssetDetailsPanel::RenderBoneEditingPlaceholder(FSkeletalMeshEditorState &State)
{
    if (FEditorUIStyle::BeginDetailsSection("Skeleton / Pose / Bone Gizmo"))
    {
        ImGui::TextDisabled("김형도 담당 영역 Placeholder");
        ImGui::BulletText("Bone local / world transform inspector 예정");
        ImGui::BulletText("Bone Gizmo 조작 예정");
        ImGui::BulletText("Pose dirty 표시 후 skinning refresh 요청 예정");

        ImGui::Separator();
        ImGui::Checkbox("Pose Edit Mode", &State.bEnablePoseEditMode);
        ImGui::Text("Selected Bone Index: %d", State.SelectedBoneIndex);
    }
}
