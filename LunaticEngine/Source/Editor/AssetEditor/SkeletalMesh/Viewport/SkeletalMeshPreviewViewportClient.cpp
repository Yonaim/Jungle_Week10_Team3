#include "AssetEditor/SkeletalMesh/Viewport/SkeletalMeshPreviewViewportClient.h"

#include "AssetEditor/SkeletalMesh/Gizmo/BoneTransformProxy.h"
#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"

#include "Component/CameraComponent.h"
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
    PreviewGizmoComponent = UObjectManager::Get().CreateObject<UGizmoComponent>();
    if (PreviewGizmoComponent)
    {
        PreviewGizmoComponent->SetScene(&PreviewScene);
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
}

const char *FSkeletalMeshPreviewViewportClient::GetViewportTooltipBarText() const
{
    return nullptr;
}

void FSkeletalMeshPreviewViewportClient::TickViewportInput(float DeltaTime)
{
    if (!PreviewCamera)
    {
        return;
    }

    ImGuiIO &IO = ImGui::GetIO();
    const bool bRightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool bMiddleMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool bAcceptViewportInput = IsHovered() || IsActive() || bRightMouseDown || bMiddleMouseDown;
    if (!bAcceptViewportInput)
    {
        return;
    }

    // 기존 Level Editor viewport와 비슷하게 Preview Viewport도 최소한의 카메라 입력을 처리한다.
    // - RMB Drag: orbit rotate
    // - MMB Drag: pan
    // - Wheel: zoom
    // - RMB + WASD/QE: orbit target 이동
    // - F: frame mesh
    if (IsLegacyImGuiKeyPressed('F'))
    {
        FramePreviewMesh();
    }

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

    if (IO.MouseWheel != 0.0f && IsHovered())
    {
        OrbitDistance = (std::max)(0.15f, OrbitDistance * (1.0f - IO.MouseWheel * 0.08f));
    }

    if (bRightMouseDown)
    {
        FVector Move = FVector::ZeroVector;
        if (IsLegacyImGuiKeyDown('W'))
        {
            Move += PreviewCamera->GetForwardVector();
        }
        if (IsLegacyImGuiKeyDown('S'))
        {
            Move -= PreviewCamera->GetForwardVector();
        }
        if (IsLegacyImGuiKeyDown('D'))
        {
            Move += PreviewCamera->GetRightVector();
        }
        if (IsLegacyImGuiKeyDown('A'))
        {
            Move -= PreviewCamera->GetRightVector();
        }
        if (IsLegacyImGuiKeyDown('E'))
        {
            Move += PreviewCamera->GetUpVector();
        }
        if (IsLegacyImGuiKeyDown('Q'))
        {
            Move -= PreviewCamera->GetUpVector();
        }

        if (!Move.IsNearlyZero())
        {
            const float MoveLength = std::sqrt(Move.X * Move.X + Move.Y * Move.Y + Move.Z * Move.Z);
            if (MoveLength > 0.0001f)
            {
                const float MoveSpeed = (std::max)(0.5f, OrbitDistance) * 1.5f;
                OrbitTarget += Move * ((MoveSpeed * DeltaTime) / MoveLength);
            }
        }
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

    SyncGizmoTargetFromSelection();

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

		RenderSkeletonDebugOverlay();   // 추가
        RenderBoneGizmoOverlay();
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

	RenderSkeletonDebugOverlay();
    RenderBoneGizmoOverlay();
    RenderFallbackOverlay();
}

void FSkeletalMeshPreviewViewportClient::RenderFallbackOverlay()
{
  //  const FRect &R = ViewportScreenRect;
  //  if (R.Width <= 0.0f || R.Height <= 0.0f)
  //  {
  //      return;
  //  }

  //  ImGui::SetCursorScreenPos(ImVec2(R.X + 16.0f, R.Y + 16.0f));
  //  ImGui::BeginGroup();
  //  ImGui::TextUnformatted("Skeletal Mesh Preview Viewport");

  //  if (!PreviewMesh)
  //  {
  //      ImGui::TextDisabled("No SkeletalMesh loaded.");
  //  }
  //  else if (!State || State->bShowMeshStatsOverlay)
  //  {
  //      ImGui::Text("Preview Mode: %s", State ? PreviewModeToText(State->PreviewMode) : "Unknown");
  //      ImGui::Text("Bones: %d", PreviewMesh->GetBoneCount());
  //      ImGui::Text("Vertices: %d", PreviewMesh->GetVertexCount());
  //      ImGui::Text("Indices: %d", PreviewMesh->GetIndexCount());
  //      if (PreviewComponent)
  //      {
  //          TArray<FNormalVertex> *Skinned = PreviewComponent->GetCPUSkinnedVertices();
  //          ImGui::Text("CPU Skinned Vertices: %d", Skinned ? static_cast<int32>(Skinned->size()) : 0);
  //      }
  //      if (State)
  //      {
  //          ImGui::Text("Selected Bone: %d", State->SelectedBoneIndex);
  //          ImGui::Text("LOD: %d", State->CurrentLODIndex);
  //          ImGui::Text("Show Bones: %s", State->bShowBones ? "true" : "false");
		//	ImGui::Text("Pose Edit Mode: %s", State->bEnablePoseEditMode ? "true" : "false");
  //      }

		//// 임시 Bone 회전 테스트용 함수 호출
		//RenderPoseEditDebugControls();
  //  }

  //  ImGui::TextDisabled("RMB Drag: Orbit / MMB Drag: Pan / Wheel: Zoom / Toolbar: Frame");
  //  ImGui::EndGroup();

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
	else
	{
		ImGui::Text("Bones: %d", PreviewMesh->GetBoneCount());

		if (State)
		{
			ImGui::Text("Selected Bone: %d", State->SelectedBoneIndex);
			ImGui::Text("Show Bones: %s", State->bShowBones ? "true" : "false");
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

	ImGui::TextDisabled("RMB Drag: Orbit / MMB Drag: Pan / Wheel: Zoom / Toolbar: Frame");
	ImGui::EndGroup();
}

// 임시 Bone 회전 테스트용 함수
void FSkeletalMeshPreviewViewportClient::RenderPoseEditDebugControls()
{
	if (!State || !State->bEnablePoseEditMode)
	{
		return;
	}

	if (!PreviewComponent || State->SelectedBoneIndex < 0)
	{
		ImGui::TextDisabled("Select a bone.");
		return;
	}

	if (ImGui::Button("Rotate Selected Bone Local Z +10 deg"))
	{
		const FSkeletonPose& Pose = PreviewComponent->GetCurrentPose();
		const int32 BoneIndex = State->SelectedBoneIndex;

		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Pose.LocalTransforms.size()))
		{
			return;
		}

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
    if (!State || !PreviewComponent || !State->bEnablePoseEditMode)
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

    if (GizmoTargetBoneIndex == SelectedBoneIndex && GizmoManager.HasValidTarget())
    {
        return;
    }

    GizmoTargetBoneIndex = SelectedBoneIndex;
    GizmoManager.SetTarget(std::make_shared<FBoneTransformProxy>(PreviewComponent, SelectedBoneIndex));
}

void FSkeletalMeshPreviewViewportClient::RenderBoneGizmoOverlay()
{
    // 3D Primitive 기반 기즈모는 PreviewScene의 UGizmoComponent가 렌더한다.
    // 이 함수는 기존 ImGui overlay 호출 지점을 유지하되, 현재는 렌더 직전/직후 상태 동기화만 수행한다.
    SyncGizmoTargetFromSelection();
}

bool FSkeletalMeshPreviewViewportClient::ProjectWorldToViewport(
	const FVector& WorldPos,
	ImVec2& OutScreen) const
{
	if (!PreviewCamera)
	{
		return false;
	}

	const FRect& R = ViewportScreenRect;
	if (R.Width <= 0.0f || R.Height <= 0.0f)
	{
		return false;
	}

	const FMatrix ViewProjection = PreviewCamera->GetViewProjectionMatrix();
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
