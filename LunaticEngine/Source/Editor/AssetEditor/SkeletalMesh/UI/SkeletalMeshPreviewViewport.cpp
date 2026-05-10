#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshPreviewViewport.h"

#include "Engine/Mesh/SkeletalMesh.h"
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

void FSkeletalMeshPreviewViewport::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, float DeltaTime)
{
    (void)DeltaTime;

    ImGui::BeginChild("##SkeletalMeshPreviewViewport", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextUnformatted("Preview Viewport");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", PreviewModeToText(State.PreviewMode));
    ImGui::Separator();

    RenderPlaceholderCanvas(Mesh, State);

    ImGui::EndChild();
}

void FSkeletalMeshPreviewViewport::RenderPlaceholderCanvas(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
    const ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
    const ImVec2 CanvasEnd(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y);

    DrawList->AddRectFilled(CanvasPos, CanvasEnd, IM_COL32(12, 12, 12, 255));
    DrawList->AddRect(CanvasPos, CanvasEnd, IM_COL32(70, 70, 70, 255));

    const ImVec2 Center((CanvasPos.x + CanvasEnd.x) * 0.5f, (CanvasPos.y + CanvasEnd.y) * 0.5f);
    const float AxisLength = 64.0f;
    DrawList->AddLine(Center, ImVec2(Center.x + AxisLength, Center.y), IM_COL32(220, 80, 80, 255), 2.0f);
    DrawList->AddLine(Center, ImVec2(Center.x, Center.y - AxisLength), IM_COL32(80, 220, 80, 255), 2.0f);
    DrawList->AddLine(Center, ImVec2(Center.x - AxisLength * 0.55f, Center.y + AxisLength * 0.55f), IM_COL32(80, 120, 240, 255), 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(CanvasPos.x + 16.0f, CanvasPos.y + 16.0f));
    if (!Mesh)
    {
        ImGui::TextDisabled("No SkeletalMesh loaded.");
    }
    else
    {
        ImGui::Text("Preview rendering placeholder");
        ImGui::TextDisabled("남윤지 담당 Runtime / CPU Skinning 렌더링이 연결되면 이 영역에 RenderTarget을 표시한다.");
        ImGui::Text("Bones: %d", Mesh->GetBoneCount());
        ImGui::Text("Vertices: %d", Mesh->GetVertexCount());
        ImGui::Text("Indices: %d", Mesh->GetIndexCount());
        ImGui::Text("Show Bones: %s", State.bShowBones ? "true" : "false");
        ImGui::Text("Selected Bone: %d", State.SelectedBoneIndex);
    }

    ImGui::Dummy(CanvasSize);
}
