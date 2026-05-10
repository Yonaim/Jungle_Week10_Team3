#include "AssetEditor/SkeletalMesh/Viewport/SkeletalMeshPreviewViewportClient.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"
#include "Viewport/Viewport.h"

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

void FSkeletalMeshPreviewViewportClient::Tick(float DeltaTime)
{
    (void)DeltaTime;

    if (State && State->bFramePreviewRequested)
    {
        // 김연하 담당 Viewer UI:
        // 현재는 실제 preview camera / bounds가 없으므로 요청만 소비한다.
        // 남윤지 담당 PreviewComponent가 연결되면 mesh bounds 기준 카메라 framing을 여기서 수행한다.
        State->bFramePreviewRequested = false;
    }

    // 남윤지 담당 영역 Placeholder:
    // PreviewComponent->TickComponent(DeltaTime) 또는 CPU Skinning 결과 갱신을 여기서 호출한다.
}

void FSkeletalMeshPreviewViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
    // 실제 RenderTarget이 연결되면 공통 FEditorViewportClient의 이미지 렌더링 경로를 그대로 사용한다.
    if (Viewport && Viewport->GetSRV())
    {
        FEditorViewportClient::RenderViewportImage(bIsActiveViewport);
        RenderFallbackOverlay();
        return;
    }

    // 아직 Preview Scene / RenderTarget이 없을 때도 Preview Viewport 패널 구조는 확인할 수 있게 fallback을 그린다.
    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0.0f || R.Height <= 0.0f)
    {
        return;
    }

    const ImVec2 Min(R.X, R.Y);
    const ImVec2 Max(R.X + R.Width, R.Y + R.Height);
    DrawList->AddRectFilled(Min, Max, IM_COL32(12, 12, 12, 255));
    DrawList->AddRect(Min, Max, IM_COL32(72, 72, 72, 255));

    const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
    const float AxisLength = 64.0f;
    DrawList->AddLine(Center, ImVec2(Center.x + AxisLength, Center.y), IM_COL32(220, 80, 80, 255), 2.0f);
    DrawList->AddLine(Center, ImVec2(Center.x, Center.y - AxisLength), IM_COL32(80, 220, 80, 255), 2.0f);
    DrawList->AddLine(Center, ImVec2(Center.x - AxisLength * 0.55f, Center.y + AxisLength * 0.55f), IM_COL32(80, 120, 240, 255), 2.0f);

    RenderFallbackOverlay();
}

void FSkeletalMeshPreviewViewportClient::RenderFallbackOverlay()
{
    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0.0f || R.Height <= 0.0f)
    {
        return;
    }

    ImGui::SetCursorScreenPos(ImVec2(R.X + 16.0f, R.Y + 16.0f));
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Skeletal Mesh Preview Viewport");

    if (!PreviewMesh)
    {
        ImGui::TextDisabled("No SkeletalMesh loaded.");
    }
    else if (!State || State->bShowMeshStatsOverlay)
    {
        ImGui::Text("Preview Mode: %s", State ? PreviewModeToText(State->PreviewMode) : "Unknown");
        ImGui::Text("Bones: %d", PreviewMesh->GetBoneCount());
        ImGui::Text("Vertices: %d", PreviewMesh->GetVertexCount());
        ImGui::Text("Indices: %d", PreviewMesh->GetIndexCount());
        if (State)
        {
            ImGui::Text("Selected Bone: %d", State->SelectedBoneIndex);
            ImGui::Text("LOD: %d", State->CurrentLODIndex);
            ImGui::Text("Show Bones: %s", State->bShowBones ? "true" : "false");
        }
        ImGui::TextDisabled("Runtime / CPU Skinning RenderTarget is not connected yet.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("남윤지 담당 Placeholder: USkeletalMeshComponent / CPU Skinning 렌더링 연결 예정");
    ImGui::TextDisabled("김형도 담당 Placeholder: Skeleton Debug Draw / Bone Gizmo 연결 예정");
    ImGui::EndGroup();
}
