#include "AssetEditor/SkeletalMesh/Viewport/SkeletalMeshPreviewViewportClient.h"

#include "Component/CameraComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Object/Object.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cmath>

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

float ClampFloat(float Value, float MinValue, float MaxValue)
{
    return (std::max)(MinValue, (std::min)(Value, MaxValue));
}

FVector MakeOrbitCameraLocation(const FVector &Target, float Distance, float YawDeg, float PitchDeg)
{
    constexpr float DegToRad = 3.14159265358979323846f / 180.0f;
    const float Yaw = YawDeg * DegToRad;
    const float Pitch = PitchDeg * DegToRad;

    const float CP = std::cos(Pitch);
    const float SP = std::sin(Pitch);
    const float CY = std::cos(Yaw);
    const float SY = std::sin(Yaw);

    const FVector Forward(CP * CY, CP * SY, SP);
    return Target - Forward * Distance;
}
} // namespace

FSkeletalMeshPreviewViewportClient::~FSkeletalMeshPreviewViewportClient()
{
    Shutdown();
}

void FSkeletalMeshPreviewViewportClient::Init(FWindowsWindow *InWindow)
{
    FEditorViewportClient::Init(InWindow);
    EnsurePreviewObjects();
}

void FSkeletalMeshPreviewViewportClient::Shutdown()
{
    ReleasePreviewObjects();
    FEditorViewportClient::Shutdown();
}

void FSkeletalMeshPreviewViewportClient::EnsurePreviewObjects()
{
    if (bPreviewObjectsInitialized)
    {
        return;
    }

    PreviewCamera = UObjectManager::Get().CreateObject<UCameraComponent>();
    PreviewComponent = UObjectManager::Get().CreateObject<USkeletalMeshComponent>();

    RenderOptions.ViewMode = EViewMode::Lit_Phong;
    RenderOptions.ShowFlags.bGrid = true;
    RenderOptions.ShowFlags.bWorldAxis = true;
    RenderOptions.ShowFlags.bGizmo = false;
    RenderOptions.ShowFlags.bSceneBVH = false;
    RenderOptions.ShowFlags.bOctree = false;
    RenderOptions.ShowFlags.bWorldBound = false;
    RenderOptions.ShowFlags.bLightVisualization = false;
    RenderOptions.GridSpacing = 1.0f;
    RenderOptions.GridHalfLineCount = 20;

    ResetPreviewCamera();
    bPreviewObjectsInitialized = true;
}

void FSkeletalMeshPreviewViewportClient::ReleasePreviewObjects()
{
    if (PreviewProxy)
    {
        PreviewScene.RemovePrimitive(PreviewProxy);
        PreviewProxy = nullptr;
    }

    if (PreviewComponent)
    {
        UObjectManager::Get().DestroyObject(PreviewComponent);
        PreviewComponent = nullptr;
    }

    if (PreviewCamera)
    {
        UObjectManager::Get().DestroyObject(PreviewCamera);
        PreviewCamera = nullptr;
    }

    PreviewMesh = nullptr;
    bPreviewObjectsInitialized = false;
}

void FSkeletalMeshPreviewViewportClient::SetPreviewMesh(USkeletalMesh *InMesh)
{
    EnsurePreviewObjects();

    if (PreviewMesh == InMesh)
    {
        return;
    }

    PreviewMesh = InMesh;
    if (PreviewComponent)
    {
        PreviewComponent->SetSkeletalMesh(PreviewMesh);
    }

    RebuildPreviewProxy();
    FramePreviewMesh();
}

void FSkeletalMeshPreviewViewportClient::RebuildPreviewProxy()
{
    if (PreviewProxy)
    {
        PreviewScene.RemovePrimitive(PreviewProxy);
        PreviewProxy = nullptr;
    }

    if (!PreviewComponent || !PreviewMesh)
    {
        return;
    }

    // PreviewComponent는 Level World에 등록하지 않는다.
    // 대신 PreviewScene에 직접 PrimitiveSceneProxy를 등록해 renderer가 같은 DrawCommand 경로를 타게 한다.
    PreviewProxy = PreviewScene.AddPrimitive(PreviewComponent);
}

void FSkeletalMeshPreviewViewportClient::ResetPreviewCamera()
{
    OrbitTarget = FVector::ZeroVector;
    OrbitDistance = 6.0f;
    OrbitYaw = 180.0f;
    OrbitPitch = -10.0f;

    if (!PreviewCamera)
    {
        return;
    }

    const FVector CameraLocation = MakeOrbitCameraLocation(OrbitTarget, OrbitDistance, OrbitYaw, OrbitPitch);
    PreviewCamera->SetWorldLocation(CameraLocation);
    PreviewCamera->LookAt(OrbitTarget);
    PreviewCamera->SetFOV(60.0f * 3.14159265358979323846f / 180.0f);
    PreviewCamera->SetOrthographic(false);
}

void FSkeletalMeshPreviewViewportClient::FramePreviewMesh()
{
    if (!PreviewMesh || !PreviewMesh->GetSkeletalMeshAsset())
    {
        ResetPreviewCamera();
        return;
    }

    const FSkeletalMesh *MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
    if (!MeshAsset || MeshAsset->Vertices.empty())
    {
        ResetPreviewCamera();
        return;
    }

    FVector Min = MeshAsset->Vertices[0].pos;
    FVector Max = MeshAsset->Vertices[0].pos;
    for (const FNormalVertex &Vertex : MeshAsset->Vertices)
    {
        Min.X = (std::min)(Min.X, Vertex.pos.X);
        Min.Y = (std::min)(Min.Y, Vertex.pos.Y);
        Min.Z = (std::min)(Min.Z, Vertex.pos.Z);
        Max.X = (std::max)(Max.X, Vertex.pos.X);
        Max.Y = (std::max)(Max.Y, Vertex.pos.Y);
        Max.Z = (std::max)(Max.Z, Vertex.pos.Z);
    }

    OrbitTarget = (Min + Max) * 0.5f;
    const FVector Extent = (Max - Min) * 0.5f;
    const float Radius = (std::max)(0.5f, std::sqrt(Extent.X * Extent.X + Extent.Y * Extent.Y + Extent.Z * Extent.Z));
    OrbitDistance = Radius * 2.8f;

    if (PreviewCamera)
    {
        const FVector CameraLocation = MakeOrbitCameraLocation(OrbitTarget, OrbitDistance, OrbitYaw, OrbitPitch);
        PreviewCamera->SetWorldLocation(CameraLocation);
        PreviewCamera->LookAt(OrbitTarget);
    }
}

void FSkeletalMeshPreviewViewportClient::Tick(float DeltaTime)
{
    EnsurePreviewObjects();
    TickViewportInput(DeltaTime);

    if (State && State->bFramePreviewRequested)
    {
        FramePreviewMesh();
        State->bFramePreviewRequested = false;
    }

    if (PreviewComponent)
    {
        PreviewComponent->RefreshSkinningForEditor(DeltaTime);
    }
}

void FSkeletalMeshPreviewViewportClient::TickViewportInput(float DeltaTime)
{
    (void)DeltaTime;

    if (!PreviewCamera || (!IsHovered() && !IsActive()))
    {
        return;
    }

    ImGuiIO &IO = ImGui::GetIO();

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        const ImVec2 Delta = IO.MouseDelta;
        OrbitYaw += Delta.x * 0.25f;
        OrbitPitch = ClampFloat(OrbitPitch - Delta.y * 0.25f, -85.0f, 85.0f);
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 Delta = IO.MouseDelta;
        const FVector Right = PreviewCamera->GetRightVector();
        const FVector Up = PreviewCamera->GetUpVector();
        const float PanScale = OrbitDistance * 0.0015f;
        OrbitTarget = OrbitTarget - Right * (Delta.x * PanScale) + Up * (Delta.y * PanScale);
    }

    if (IO.MouseWheel != 0.0f)
    {
        OrbitDistance = (std::max)(0.15f, OrbitDistance * (1.0f - IO.MouseWheel * 0.08f));
    }

    const FVector CameraLocation = MakeOrbitCameraLocation(OrbitTarget, OrbitDistance, OrbitYaw, OrbitPitch);
    PreviewCamera->SetWorldLocation(CameraLocation);
    PreviewCamera->LookAt(OrbitTarget);
}

bool FSkeletalMeshPreviewViewportClient::BuildRenderRequest(FEditorViewportRenderRequest &OutRequest)
{
    EnsurePreviewObjects();

    if (!Viewport || !PreviewCamera || !PreviewComponent || !PreviewMesh || !PreviewProxy)
    {
        return false;
    }

    OutRequest.Viewport = Viewport;
    OutRequest.Camera = PreviewCamera;
    OutRequest.Scene = &PreviewScene;
    OutRequest.RenderOptions = RenderOptions;
    OutRequest.CursorProvider = this;
    OutRequest.bRenderGrid = State ? State->bShowGrid : true;
    OutRequest.bEnableGPUOcclusion = false;
    OutRequest.bAllowLevelDebugVisuals = false;
    return true;
}

void FSkeletalMeshPreviewViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
    // Preview Scene RenderTarget이 연결되면 공통 FEditorViewportClient 이미지 출력 경로를 그대로 사용한다.
    if (Viewport && Viewport->GetSRV())
    {
        FEditorViewportClient::RenderViewportImage(bIsActiveViewport);
        RenderFallbackOverlay();
        return;
    }

    // RenderPipeline 연결 전 또는 렌더 실패 시에도 패널이 비어 보이지 않도록 fallback을 유지한다.
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
        if (PreviewComponent)
        {
            TArray<FNormalVertex> *Skinned = PreviewComponent->GetCPUSkinnedVertices();
            ImGui::Text("CPU Skinned Vertices: %d", Skinned ? static_cast<int32>(Skinned->size()) : 0);
        }
        if (State)
        {
            ImGui::Text("Selected Bone: %d", State->SelectedBoneIndex);
            ImGui::Text("LOD: %d", State->CurrentLODIndex);
            ImGui::Text("Show Bones: %s", State->bShowBones ? "true" : "false");
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("RMB Drag: Orbit / MMB Drag: Pan / Wheel: Zoom / Toolbar: Frame");
    ImGui::EndGroup();
}
