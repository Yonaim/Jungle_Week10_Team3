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
} // namespace

void FSkeletalMeshDetailsPanel::Render(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State)
{
    ImGui::BeginChild("##SkeletalMeshDetailsPanel", ImVec2(0.0f, 0.0f), true);

    ImGui::TextUnformatted("Details");
    ImGui::Separator();

    if (!Mesh)
    {
        ImGui::TextDisabled("No SkeletalMesh asset selected.");
        ImGui::EndChild();
        return;
    }

    RenderMeshInfo(Mesh, AssetPath, State);
    ImGui::Spacing();
    RenderRuntimePlaceholder();
    ImGui::Spacing();
    RenderBoneEditingPlaceholder(State);

    ImGui::EndChild();
}

void FSkeletalMeshDetailsPanel::RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State)
{
    if (ImGui::CollapsingHeader("Mesh Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const FString FileName = AssetPath.empty() ? FString("Untitled") : FPaths::ToUtf8(AssetPath.filename().wstring());
        const FString FullPath = AssetPath.empty() ? FString("") : FPaths::ToUtf8(AssetPath.wstring());

        ImGui::Text("Asset: %s", FileName.c_str());
        if (!FullPath.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", FullPath.c_str());
        }

        ImGui::Text("Bone Count: %d", Mesh->GetBoneCount());
        ImGui::Text("Vertex Count: %d", Mesh->GetVertexCount());
        ImGui::Text("Index Count: %d", Mesh->GetIndexCount());
        ImGui::Text("Current LOD: %d", State.CurrentLODIndex);
        ImGui::Text("Selected Bone: %d", State.SelectedBoneIndex);
        ImGui::Text("Preview Mode: %s", PreviewModeToText(State.PreviewMode));
    }
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
