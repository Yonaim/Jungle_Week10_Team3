#include "AssetEditor/SkeletalMesh/Viewport/SkeletalMeshPreviewViewportClient.h"

#include "AssetEditor/SkeletalMesh/Gizmo/BoneTransformGizmoTarget.h"
#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"

#include "Component/GizmoComponent.h"
#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Object/Object.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Viewport/Viewport.h"
#include "Math/Rotator.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <cmath>
#include <memory>

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

bool IsLegacyImGuiKeyDown(int Key)
{
    const ImGuiKey ImKey = [&]() {
        switch (Key)
        {
        case 'A': return ImGuiKey_A;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case ' ': return ImGuiKey_Space;
        case 'Q': return ImGuiKey_Q;
        case 'S': return ImGuiKey_S;
        case 'W': return ImGuiKey_W;
        default: return ImGuiKey_None;
        }
    }();
    return ImKey != ImGuiKey_None && ImGui::IsKeyDown(ImKey);
}

bool IsLegacyImGuiKeyPressed(int Key)
{
    const ImGuiKey ImKey = [&]() {
        switch (Key)
        {
        case 'A': return ImGuiKey_A;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case ' ': return ImGuiKey_Space;
        case 'Q': return ImGuiKey_Q;
        case 'S': return ImGuiKey_S;
        case 'W': return ImGuiKey_W;
        default: return ImGuiKey_None;
        }
    }();
    return ImKey != ImGuiKey_None && ImGui::IsKeyPressed(ImKey, false);
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

EViewMode ToRuntimeViewMode(ESkeletalMeshPreviewViewMode Mode)
{
    switch (Mode)
    {
    case ESkeletalMeshPreviewViewMode::Unlit: return EViewMode::Unlit;
    case ESkeletalMeshPreviewViewMode::LitGouraud: return EViewMode::Lit_Gouraud;
    case ESkeletalMeshPreviewViewMode::LitLambert: return EViewMode::Lit_Lambert;
    case ESkeletalMeshPreviewViewMode::Wireframe: return EViewMode::Wireframe;
    case ESkeletalMeshPreviewViewMode::SceneDepth: return EViewMode::SceneDepth;
    case ESkeletalMeshPreviewViewMode::WorldNormal: return EViewMode::WorldNormal;
    case ESkeletalMeshPreviewViewMode::LightCulling: return EViewMode::LightCulling;
    case ESkeletalMeshPreviewViewMode::Lit:
    default: return EViewMode::Lit_Phong;
    }
}

ELevelViewportType ToRuntimeViewportType(ESkeletalMeshPreviewViewportType Type)
{
    switch (Type)
    {
    case ESkeletalMeshPreviewViewportType::Top: return ELevelViewportType::Top;
    case ESkeletalMeshPreviewViewportType::Bottom: return ELevelViewportType::Bottom;
    case ESkeletalMeshPreviewViewportType::Left: return ELevelViewportType::Left;
    case ESkeletalMeshPreviewViewportType::Right: return ELevelViewportType::Right;
    case ESkeletalMeshPreviewViewportType::Front: return ELevelViewportType::Front;
    case ESkeletalMeshPreviewViewportType::Back: return ELevelViewportType::Back;
    case ESkeletalMeshPreviewViewportType::FreeOrtho: return ELevelViewportType::FreeOrthographic;
    case ESkeletalMeshPreviewViewportType::Perspective:
    default: return ELevelViewportType::Perspective;
    }
}

FRotator GetFixedViewportRotation(ESkeletalMeshPreviewViewportType Type)
{
    switch (Type)
    {
    case ESkeletalMeshPreviewViewportType::Top: return FRotator(90.0f, 0.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Bottom: return FRotator(-90.0f, 0.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Left: return FRotator(0.0f, 90.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Right: return FRotator(0.0f, -90.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Front: return FRotator(0.0f, 180.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::Back: return FRotator(0.0f, 0.0f, 0.0f);
    case ESkeletalMeshPreviewViewportType::FreeOrtho:
    case ESkeletalMeshPreviewViewportType::Perspective:
    default: return FRotator::ZeroRotator;
    }
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

    PreviewComponent = UObjectManager::Get().CreateObject<USkeletalMeshComponent>();
    PreviewGizmoComponent = UObjectManager::Get().CreateObject<UGizmoComponent>();
    if (PreviewGizmoComponent)
    {
        PreviewGizmoComponent->SetScene(&PreviewScene.GetScene());
        PreviewGizmoComponent->SetGizmoSpace(EGizmoSpace::Local);
        PreviewGizmoComponent->CreateRenderState();
        PreviewGizmoComponent->ClearGizmoWorldTransform();
        GizmoManager.SetVisualComponent(PreviewGizmoComponent);
    }

    RenderOptions.ViewMode = EViewMode::Lit_Phong;
    RenderOptions.ShowFlags.bGrid = true;
    RenderOptions.ShowFlags.bWorldAxis = true;
    RenderOptions.ShowFlags.bGizmo = true;
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

    GizmoManager.ClearTarget();
    GizmoManager.SetVisualComponent(nullptr);

    if (PreviewGizmoComponent)
    {
        PreviewGizmoComponent->DestroyRenderState();
        UObjectManager::Get().DestroyObject(PreviewGizmoComponent);
        PreviewGizmoComponent = nullptr;
    }

    if (PreviewComponent)
    {
        UObjectManager::Get().DestroyObject(PreviewComponent);
        PreviewComponent = nullptr;
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
    GizmoManager.ClearTarget();
    GizmoTargetBoneIndex = -1;
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


void FSkeletalMeshPreviewViewportClient::ApplyEditorStateToViewport()
{
    SyncRenderOptionsFromState();
    ApplyViewportTypeToCamera();

    if (PreviewGizmoComponent && State)
    {
        PreviewGizmoComponent->SetSnapSettings(
            State->bEnableTranslationSnap, State->TranslationSnapSize,
            State->bEnableRotationSnap, State->RotationSnapSize,
            State->bEnableScaleSnap, State->ScaleSnapSize);
    }
}

void FSkeletalMeshPreviewViewportClient::SyncRenderOptionsFromState()
{
    if (!State)
    {
        return;
    }

    RenderOptions.ViewMode = ToRuntimeViewMode(State->PreviewViewMode);
    RenderOptions.ViewportType = ToRuntimeViewportType(State->PreviewViewportType);

    RenderOptions.ShowFlags.bPrimitives = State->bShowPrimitives;
    RenderOptions.ShowFlags.bGrid = State->bShowGrid;
    RenderOptions.ShowFlags.bWorldAxis = State->bShowWorldAxis;
    RenderOptions.ShowFlags.bGizmo = State->bShowGizmo;
    RenderOptions.ShowFlags.bBillboardText = State->bShowBillboardText;
    RenderOptions.ShowFlags.bSkeletalMesh = State->bShowSkeletalMesh;
    RenderOptions.ShowFlags.bSceneBVH = State->bShowSceneBVH;
    RenderOptions.ShowFlags.bOctree = State->bShowOctree;
    RenderOptions.ShowFlags.bWorldBound = State->bShowWorldBound;
    RenderOptions.ShowFlags.bLightVisualization = State->bShowLightVisualization;
    RenderOptions.ShowFlags.bViewLightCulling = State->PreviewViewMode == ESkeletalMeshPreviewViewMode::LightCulling;

    RenderOptions.GridSpacing = State->GridSpacing;
    RenderOptions.GridHalfLineCount = State->GridHalfLineCount;
    RenderOptions.GridRenderSettings.LineThickness = State->GridLineThickness;
    RenderOptions.GridRenderSettings.MajorLineThickness = State->GridMajorLineThickness;
    RenderOptions.GridRenderSettings.MajorLineInterval = State->GridMajorLineInterval;
    RenderOptions.GridRenderSettings.MinorIntensity = State->GridMinorIntensity;
    RenderOptions.GridRenderSettings.MajorIntensity = State->GridMajorIntensity;
    RenderOptions.GridRenderSettings.AxisThickness = State->AxisThickness;
    RenderOptions.GridRenderSettings.AxisIntensity = State->AxisIntensity;
    RenderOptions.DebugLineThickness = State->DebugLineThickness;
    RenderOptions.ActorHelperBillboardScale = State->BillboardIconScale;
    RenderOptions.CameraMoveSensitivity = State->CameraSpeed;
}

void FSkeletalMeshPreviewViewportClient::ApplyViewportTypeToCamera()
{
    if (!State)
    {
        return;
    }

    const bool bViewportTypeChanged = !bHasAppliedViewportType || LastAppliedViewportType != State->PreviewViewportType;
    bHasAppliedViewportType = true;
    LastAppliedViewportType = State->PreviewViewportType;

    constexpr float DegToRad = 3.14159265358979323846f / 180.0f;
    ViewCamera.SetFOV(State->CameraFOV * DegToRad);
    ViewCamera.SetOrthoWidth((std::max)(0.1f, State->CameraOrthoWidth));

    const bool bOrtho = State->PreviewViewportType != ESkeletalMeshPreviewViewportType::Perspective;
    ViewCamera.SetOrthographic(bOrtho);

    if (!bOrtho)
    {
        return;
    }

    if (State->PreviewViewportType == ESkeletalMeshPreviewViewportType::FreeOrtho)
    {
        // Free Ortho는 현재 orbit 회전은 유지하고 projection만 ortho로 바꾼다.
        return;
    }

    const FRotator Rotation = GetFixedViewportRotation(State->PreviewViewportType);
    ViewCamera.SetWorldRotation(Rotation);
    const FVector Forward = ViewCamera.GetForwardVector();
    const float Distance = (std::max)(1.0f, OrbitDistance);
    ViewCamera.SetWorldLocation(OrbitTarget - Forward * Distance);

    if (bViewportTypeChanged)
    {
        // 고정 ortho 전환 시 기존 orbit 각도와 상관없이 대상 중심을 유지한다.
        State->CameraOrthoWidth = (std::max)(State->CameraOrthoWidth, OrbitDistance * 2.0f);
        ViewCamera.SetOrthoWidth(State->CameraOrthoWidth);
    }
}

void FSkeletalMeshPreviewViewportClient::ResetPreviewCamera()
{
    OrbitTarget = FVector::ZeroVector;
    OrbitDistance = 6.0f;
    OrbitYaw = 180.0f;
    OrbitPitch = -10.0f;

    const FVector CameraLocation = MakeOrbitCameraLocation(OrbitTarget, OrbitDistance, OrbitYaw, OrbitPitch);
    ViewCamera.SetWorldLocation(CameraLocation);
    ViewCamera.LookAt(OrbitTarget);
    ViewCamera.SetFOV(60.0f * 3.14159265358979323846f / 180.0f);
    ViewCamera.SetOrthographic(false);
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

    const FVector CameraLocation = MakeOrbitCameraLocation(OrbitTarget, OrbitDistance, OrbitYaw, OrbitPitch);
    ViewCamera.SetWorldLocation(CameraLocation);
    ViewCamera.LookAt(OrbitTarget);
}

void FSkeletalMeshPreviewViewportClient::Tick(float DeltaTime)
{
    EnsurePreviewObjects();
    ApplyEditorStateToViewport();
    SyncGizmoTargetFromSelection();
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

    GizmoManager.ApplyScreenSpaceScaling(
        ViewCamera.GetWorldLocation(),
        ViewCamera.IsOrthogonal(),
        ViewCamera.GetOrthoWidth());
}

const char *FSkeletalMeshPreviewViewportClient::GetViewportTooltipBarText() const
{
    return nullptr;
}


void FSkeletalMeshPreviewViewportClient::CycleGizmoModeFromShortcut()
{
    if (!State)
    {
        return;
    }

    switch (State->GizmoMode)
    {
    case EGizmoMode::Translate:
        State->GizmoMode = EGizmoMode::Rotate;
        break;
    case EGizmoMode::Rotate:
        State->GizmoMode = EGizmoMode::Scale;
        break;
    case EGizmoMode::Scale:
    default:
        State->GizmoMode = EGizmoMode::Translate;
        break;
    }

    GizmoManager.SetMode(State->GizmoMode);
    if (PreviewGizmoComponent)
    {
        PreviewGizmoComponent->UpdateGizmoMode(State->GizmoMode);
    }
}

void FSkeletalMeshPreviewViewportClient::TickViewportInput(float DeltaTime)
{
    ImGuiIO &IO = ImGui::GetIO();
    const bool bRightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool bMiddleMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool bAcceptViewportInput = IsHovered() || IsActive() || bRightMouseDown || bMiddleMouseDown;
    if (!bAcceptViewportInput)
    {
        return;
    }

    // 기존 Level Editor 뷰포트처럼 Preview Viewport도 최소한의 카메라 입력을 처리한다.
    // - RMB 드래그: 오비트 회전
    // - MMB 드래그: 팬 이동
    // - 휠: 줌
    // - RMB + WASD/QE: 오비트 타깃 이동
    // - F: 메시 프레이밍
    // - Space: 기즈모 모드 순환
    if (IsLegacyImGuiKeyPressed('F'))
    {
        FramePreviewMesh();
    }

    if (IsLegacyImGuiKeyPressed(' '))
    {
        CycleGizmoModeFromShortcut();
    }

    // 김형도 담당 예정:
    // Bone gizmo의 실제 피킹/드래그/pose transform 적용은 Skeleton/Pose/Bone Gizmo 파트에서 구현한다.
    // 김연하 담당 범위에서는 선택한 bone 위치에 UGizmoComponent를 렌더링하고,
    // toolbar/shortcut으로 표시 모드만 바꾸는 데서 멈춘다.
    // 따라서 SkeletalMeshEditor preview에서는 BeginDrag / UpdateDrag / EndDrag를 호출하지 않는다.
    if (GizmoManager.IsDragging())
    {
        GizmoManager.CancelDrag();
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && (!State || State->PreviewViewportType == ESkeletalMeshPreviewViewportType::Perspective || State->PreviewViewportType == ESkeletalMeshPreviewViewportType::FreeOrtho))
    {
        const ImVec2 Delta = IO.MouseDelta;
        OrbitYaw += Delta.x * 0.25f;
        OrbitPitch = ClampFloat(OrbitPitch - Delta.y * 0.25f, -85.0f, 85.0f);
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        const ImVec2 Delta = IO.MouseDelta;
        const FVector Right = ViewCamera.GetRightVector();
        const FVector Up = ViewCamera.GetUpVector();
        const float PanScale = OrbitDistance * 0.0015f;
        OrbitTarget = OrbitTarget - Right * (Delta.x * PanScale) + Up * (Delta.y * PanScale);
    }

    if (IO.MouseWheel != 0.0f && IsHovered())
    {
        OrbitDistance = (std::max)(0.15f, OrbitDistance * (1.0f - IO.MouseWheel * 0.08f));
    }

    if (bRightMouseDown)
    {
        FVector Move = FVector::ZeroVector;
        if (IsLegacyImGuiKeyDown('W'))
        {
            Move += ViewCamera.GetForwardVector();
        }
        if (IsLegacyImGuiKeyDown('S'))
        {
            Move -= ViewCamera.GetForwardVector();
        }
        if (IsLegacyImGuiKeyDown('D'))
        {
            Move += ViewCamera.GetRightVector();
        }
        if (IsLegacyImGuiKeyDown('A'))
        {
            Move -= ViewCamera.GetRightVector();
        }
        if (IsLegacyImGuiKeyDown('E'))
        {
            Move += ViewCamera.GetUpVector();
        }
        if (IsLegacyImGuiKeyDown('Q'))
        {
            Move -= ViewCamera.GetUpVector();
        }

        if (!Move.IsNearlyZero())
        {
            const float MoveLength = std::sqrt(Move.X * Move.X + Move.Y * Move.Y + Move.Z * Move.Z);
            if (MoveLength > 0.0001f)
            {
                const float CameraSpeed = State ? State->CameraSpeed : 5.0f;
                const float MoveSpeed = (std::max)(0.5f, OrbitDistance) * (std::max)(0.1f, CameraSpeed);
                OrbitTarget += Move * ((MoveSpeed * DeltaTime) / MoveLength);
            }
        }
    }

    if (!State || State->PreviewViewportType == ESkeletalMeshPreviewViewportType::Perspective || State->PreviewViewportType == ESkeletalMeshPreviewViewportType::FreeOrtho)
    {
        const FVector CameraLocation = MakeOrbitCameraLocation(OrbitTarget, OrbitDistance, OrbitYaw, OrbitPitch);
        ViewCamera.SetWorldLocation(CameraLocation);
        ViewCamera.LookAt(OrbitTarget);
    }
    ApplyViewportTypeToCamera();
}

bool FSkeletalMeshPreviewViewportClient::BuildRenderRequest(FEditorViewportRenderRequest &OutRequest)
{
    EnsurePreviewObjects();

    if (!Viewport || !PreviewComponent || !PreviewMesh || !PreviewProxy)
    {
        return false;
    }

    ApplyEditorStateToViewport();
    SyncGizmoTargetFromSelection();

    // 프리뷰 카메라가 프리뷰 씬 렌더 타깃과 동일한 렌더 범위를 사용하도록 맞춘다.
    // Asset Preview 뷰포트 크기의 기준값은 ImGui 패널 사각형이다.
    if (ViewportScreenRect.Width > 0.0f && ViewportScreenRect.Height > 0.0f)
    {
        ViewCamera.OnResize(static_cast<int32>(ViewportScreenRect.Width),
                            static_cast<int32>(ViewportScreenRect.Height));
    }

    OutRequest.Viewport = Viewport;
    OutRequest.ViewInfo = ViewCamera.GetCameraState();
    OutRequest.Scene = &PreviewScene.GetScene();
    OutRequest.RenderOptions = RenderOptions;
    OutRequest.CursorProvider = this;
    OutRequest.bRenderGrid = RenderOptions.ShowFlags.bGrid;
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

        RenderSkeletonDebugOverlay();
        RenderFallbackOverlay();
		RenderPoseEditDebugControls();
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

    RenderSkeletonDebugOverlay();
    RenderFallbackOverlay();
	RenderPoseEditDebugControls();
}

void FSkeletalMeshPreviewViewportClient::RenderFallbackOverlay()
{
	const FRect& R = ViewportScreenRect;
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
		ImGui::Text("Bones: %d", PreviewMesh->GetBoneCount());
		ImGui::Text("Vertices: %d", PreviewMesh->GetVertexCount());
		ImGui::Text("Indices: %d", PreviewMesh->GetIndexCount());

		if (State)
		{
			ImGui::Text("Selected Bone: %d", State->SelectedBoneIndex);
			ImGui::Text("LOD: %d", State->CurrentLODIndex);
			ImGui::Text("Show Bones: %s", State->bShowBones ? "true" : "false");
			ImGui::Text("Pose Edit Mode: %s", State->bEnablePoseEditMode ? "true" : "false");
		}

		if (PreviewComponent)
		{
			const FSkeletonPose& Pose = PreviewComponent->GetCurrentPose();
			ImGui::Text("Pose Transforms: %d", static_cast<int32>(Pose.ComponentTransforms.size()));

			int32 ProjectOK = 0;
			int32 ProjectFail = 0;

			const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
			if (MeshAsset)
			{
				const TArray<FBoneInfo>& Bones = MeshAsset->Bones;
				const int32 BoneCount = static_cast<int32>(Bones.size());

				for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
				{
					const FVector BonePos = Pose.ComponentTransforms[BoneIndex].GetLocation();
					ImVec2 Screen;
					if (ProjectWorldToViewport(BonePos, Screen))
					{
						++ProjectOK;
					}
					else
					{
						++ProjectFail;
					}
				}
			}

			ImGui::Text("Project OK: %d", ProjectOK);
			ImGui::Text("Project Fail: %d", ProjectFail);
		}
	}

	ImGui::TextDisabled("RMB Drag: Orbit / MMB Drag: Pan / Wheel: Zoom / F: Frame");
	ImGui::EndGroup();
}

// 임시 Bone 회전 테스트용 함수
void FSkeletalMeshPreviewViewportClient::RenderPoseEditDebugControls()
{
	if (!State || !State->bEnablePoseEditMode)
	{
		return;
	}

	const FRect& R = ViewportScreenRect;
	if (R.Width <= 0.0f || R.Height <= 0.0f)
	{
		return;
	}

	ImGui::SetNextWindowPos(ImVec2(R.X + 12.0f, R.Y + 12.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);

	const ImGuiWindowFlags Flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize;

	if (!ImGui::Begin("##SkeletalMeshPoseEditDebugControls", nullptr, Flags))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Pose Edit Debug");

	if (!PreviewComponent || State->SelectedBoneIndex < 0)
	{
		ImGui::TextDisabled("Select a bone.");
		ImGui::End();
		return;
	}

	ImGui::Text("Selected Bone: %d", State->SelectedBoneIndex);

	if (ImGui::Button("Rotate Selected Bone Local Z +10 deg"))
	{
		const FSkeletonPose& Pose = PreviewComponent->GetCurrentPose();
		const int32 BoneIndex = State->SelectedBoneIndex;

		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(Pose.LocalTransforms.size()))
		{
			FTransform Local = Pose.LocalTransforms[BoneIndex];

			constexpr float DegToRad = 3.14159265358979323846f / 180.0f;
			const FQuat AddRot = FQuat::FromAxisAngle(
				FVector(0.0f, 0.0f, 1.0f),
				10.0f * DegToRad
			);

			Local.Rotation = Local.Rotation * AddRot;
			Local.Rotation.Normalize();

			PreviewComponent->SetBoneLocalTransform(BoneIndex, Local);
			PreviewComponent->RefreshSkinningForEditor(0.0f);
		}
	}

	if (ImGui::Button("Reset Preview Pose"))
	{
		PreviewComponent->SetSkeletalMesh(PreviewMesh);
		PreviewComponent->RefreshSkinningForEditor(0.0f);
	}

	ImGui::End();
}

void FSkeletalMeshPreviewViewportClient::RenderSkeletonDebugOverlay()
{
	if (!State || !State->bShowBones)
	{
		return;
	}

	if (!PreviewMesh || !PreviewComponent)
	{
		return;
	}

	const FSkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
	if (!MeshAsset)
	{
		return;
	}

	const TArray<FBoneInfo>& Bones = MeshAsset->Bones;
	const FSkeletonPose& Pose = PreviewComponent->GetCurrentPose();

	const int32 BoneCount = static_cast<int32>(Bones.size());
	if (BoneCount <= 0)
	{
		return;
	}

	if (static_cast<int32>(Pose.ComponentTransforms.size()) != BoneCount)
	{
		return;
	}

	ImDrawList* DrawList = ImGui::GetForegroundDrawList();

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
		if (ParentIndex < 0 || ParentIndex >= BoneCount)
		{
			continue;
		}

		ImVec2 BoneScreen;
		ImVec2 ParentScreen;

		const FVector BonePos = Pose.ComponentTransforms[BoneIndex].GetLocation();
		const FVector ParentPos = Pose.ComponentTransforms[ParentIndex].GetLocation();

		if (!ProjectWorldToViewport(BonePos, BoneScreen))
		{
			continue;
		}

		if (!ProjectWorldToViewport(ParentPos, ParentScreen))
		{
			continue;
		}

		const bool bSelected =
			BoneIndex == State->SelectedBoneIndex ||
			ParentIndex == State->SelectedBoneIndex;

		DrawList->AddLine(
			ParentScreen,
			BoneScreen,
			bSelected ? IM_COL32(255, 220, 40, 255) : IM_COL32(80, 220, 255, 220),
			bSelected ? 3.0f : 1.5f
		);

		DrawList->AddCircleFilled(
			BoneScreen,
			bSelected ? 4.0f : 2.5f,
			bSelected ? IM_COL32(255, 220, 40, 255) : IM_COL32(0, 255, 0, 255)
		);
	}
}


void FSkeletalMeshPreviewViewportClient::SyncGizmoTargetFromSelection()
{
    if (!State || !PreviewComponent || !State->bShowGizmo)
    {
        GizmoManager.ClearTarget();
        GizmoTargetBoneIndex = -1;
        return;
    }

    const int32 SelectedBoneIndex = SelectionManager ? SelectionManager->GetPrimaryBoneIndex() : State->SelectedBoneIndex;
    if (SelectedBoneIndex < 0)
    {
        GizmoManager.ClearTarget();
        GizmoTargetBoneIndex = -1;
        return;
    }

    GizmoManager.SetMode(State->GizmoMode);
    GizmoManager.SetSpace(State->GizmoSpace);
    if (PreviewGizmoComponent)
    {
        PreviewGizmoComponent->SetSnapSettings(
            State->bEnableTranslationSnap, State->TranslationSnapSize,
            State->bEnableRotationSnap, State->RotationSnapSize,
            State->bEnableScaleSnap, State->ScaleSnapSize);
    }

    if (GizmoTargetBoneIndex == SelectedBoneIndex && GizmoManager.HasValidTarget())
    {
        GizmoManager.SyncVisualFromTarget();
        return;
    }

    GizmoTargetBoneIndex = SelectedBoneIndex;

    // 김형도 담당 예정:
    // 이 target은 현재 "선택 bone 위치에 기즈모를 렌더하기 위한 transform source"로만 사용한다.
    // 실제 SetWorldTransform 기반 pose edit / drag 적용은 Bone Gizmo 담당 파트에서 연결한다.
    GizmoManager.SetTarget(std::make_shared<FBoneTransformGizmoTarget>(PreviewComponent, SelectedBoneIndex));
}

bool FSkeletalMeshPreviewViewportClient::ProjectWorldToViewport(
	const FVector& WorldPos,
	ImVec2& OutScreen) const
{
		const FRect& R = ViewportScreenRect;
	if (R.Width <= 0.0f || R.Height <= 0.0f)
	{
		return false;
	}

	const FMatrix ViewProjection = ViewCamera.GetViewProjectionMatrix();
	const FVector Ndc = ViewProjection.TransformPositionWithW(WorldPos);

	if (!std::isfinite(Ndc.X) || !std::isfinite(Ndc.Y) || !std::isfinite(Ndc.Z))
	{
		return false;
	}

	float ScreenX = R.X + (Ndc.X * 0.5f + 0.5f) * R.Width;
	float ScreenY = R.Y + (-Ndc.Y * 0.5f + 0.5f) * R.Height;

	ScreenX = ClampFloat(ScreenX, R.X, R.X + R.Width);
	ScreenY = ClampFloat(ScreenY, R.Y, R.Y + R.Height);

	OutScreen = ImVec2(ScreenX, ScreenY);
	return true;
}
