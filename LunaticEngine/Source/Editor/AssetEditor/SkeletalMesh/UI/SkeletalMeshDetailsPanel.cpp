#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshDetailsPanel.h"

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

void DrawReadOnlyIntRow(const char *Label, int32 Value)
{
    ImGui::TextUnformatted(Label);
    ImGui::TableNextColumn();
    ImGui::Text("%d", Value);
    ImGui::TableNextColumn();
}

void DrawReadOnlyTextRow(const char *Label, const char *Value)
{
    ImGui::TextUnformatted(Label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(Value ? Value : "");
    ImGui::TableNextColumn();
}
} // namespace

void FSkeletalMeshDetailsPanel::Render(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State,
                                       const char *WindowName, ImGuiID DockspaceId)
{
    if (DockspaceId != 0)
    {
        ImGui::SetNextWindowDockID(DockspaceId, ImGuiCond_FirstUseEver);
    }

    if (!ImGui::Begin(WindowName, nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Details");
    ImGui::SameLine();
    ImGui::TextDisabled("Viewer / Asset Info");
    ImGui::Separator();

    if (!Mesh)
    {
        ImGui::TextDisabled("No SkeletalMesh asset selected.");
        ImGui::End();
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

    ImGui::End();
}

void FSkeletalMeshDetailsPanel::RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State)
{
    if (!ImGui::CollapsingHeader("Mesh Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    const FString FileName = AssetPath.empty() ? FString("Untitled") : FPaths::ToUtf8(AssetPath.filename().wstring());
    const FString FullPath = AssetPath.empty() ? FString("") : FPaths::ToUtf8(AssetPath.wstring());

    if (ImGui::BeginTable("##SkeletalMeshInfoTable", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextColumn();
        DrawReadOnlyTextRow("Asset", FileName.c_str());
        if (!FullPath.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", FullPath.c_str());
        }

        DrawReadOnlyIntRow("Bone Count", Mesh->GetBoneCount());
        DrawReadOnlyIntRow("Vertex Count", Mesh->GetVertexCount());
        DrawReadOnlyIntRow("Index Count", Mesh->GetIndexCount());
        DrawReadOnlyIntRow("Current LOD", State.CurrentLODIndex);
        DrawReadOnlyIntRow("Selected Bone", State.SelectedBoneIndex);
        DrawReadOnlyTextRow("Preview Mode", PreviewModeToText(State.PreviewMode));

        ImGui::EndTable();
    }
}

void FSkeletalMeshDetailsPanel::RenderLODSectionMaterialInfo(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    (void)Mesh;

    if (!ImGui::CollapsingHeader("LOD / Section / Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::TextDisabled("김연하 담당 Viewer UI 영역");
    ImGui::BulletText("현재 선택 LOD / Section / Material Slot 상태를 보관한다.");
    ImGui::BulletText("FBX / Asset Pipeline에서 LOD, Section, Material accessor가 확정되면 실제 목록으로 교체한다.");

    ImGui::Separator();

    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("Current LOD", &State.CurrentLODIndex, 1, 1))
    {
        if (State.CurrentLODIndex < 0)
        {
            State.CurrentLODIndex = 0;
        }
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

    if (ImGui::BeginTable("##SkeletalMeshLODPlaceholderTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
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

void FSkeletalMeshDetailsPanel::RenderViewerActions(FSkeletalMeshEditorState &State)
{
    if (!ImGui::CollapsingHeader("Viewer Actions", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    if (ImGui::Button("Frame Mesh"))
    {
        State.bFramePreviewRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Bone Selection"))
    {
        State.SelectedBoneIndex = -1;
    }

    ImGui::Checkbox("Show Mesh Stats Overlay", &State.bShowMeshStatsOverlay);
    ImGui::Checkbox("Show Bones", &State.bShowBones);
}

void FSkeletalMeshDetailsPanel::RenderRuntimePlaceholder()
{
    if (ImGui::CollapsingHeader("Runtime / Skinning", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("남윤지 담당 영역 Placeholder");
        ImGui::BulletText("USkinnedMeshComponent / USkeletalMeshComponent 연동 예정");
        ImGui::BulletText("Reference Pose / CPU Skinning 결과 표시 예정");
        ImGui::BulletText("Preview Viewport의 실제 렌더링 입력으로 연결 예정");
    }
}

void FSkeletalMeshDetailsPanel::RenderBoneEditingPlaceholder(FSkeletalMeshEditorState &State)
{
    if (ImGui::CollapsingHeader("Skeleton / Pose / Bone Gizmo", ImGuiTreeNodeFlags_DefaultOpen))
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
