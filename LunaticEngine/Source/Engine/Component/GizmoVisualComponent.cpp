#include "GizmoVisualComponent.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Math/Quat.h"
#include "Math/Matrix.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Shader/ShaderManager.h"
#include "Collision/RayUtils.h"
#include "Render/Proxy/GizmoSceneProxy.h"
#include "Render/Scene/FScene.h"
#include <cfloat>

IMPLEMENT_CLASS(UGizmoVisualComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UGizmoVisualComponent)

FPrimitiveSceneProxy* UGizmoVisualComponent::CreateSceneProxy()
{
	return new FGizmoSceneProxy(this, false); // Outer
}

void UGizmoVisualComponent::CreateRenderState()
{
	if (SceneProxy) return;

	FScene* Scene = RegisteredScene;
	if (!Scene && Owner && Owner->GetWorld())
		Scene = &Owner->GetWorld()->GetScene();
	if (!Scene) return;

	// Outer 프록시 (기본 경로)
	SceneProxy = Scene->AddPrimitive(this);

	// Inner 프록시 (별도 등록)
	InnerProxy = new FGizmoSceneProxy(this, true);
	Scene->RegisterProxy(InnerProxy);
}

void UGizmoVisualComponent::DestroyRenderState()
{
	FScene* Scene = RegisteredScene;
	if (!Scene && Owner && Owner->GetWorld())
		Scene = &Owner->GetWorld()->GetScene();

	if (Scene)
	{
		if (InnerProxy) { Scene->RemovePrimitive(InnerProxy); InnerProxy = nullptr; }
		if (SceneProxy) { Scene->RemovePrimitive(SceneProxy); SceneProxy = nullptr; }
	}

	// 같은 proxy를 죽은/교체된 FScene에서 다시 제거하지 않도록 명시적으로 끊어 둔다.
	RegisteredScene = nullptr;
}

#include <cmath>
UGizmoVisualComponent::UGizmoVisualComponent()
{
	MeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
	LocalExtents = FVector(1.5f, 1.5f, 1.5f);
}

void UGizmoVisualComponent::SetHolding(bool bHold)
{
    bIsHolding = bHold;
    if (!bIsHolding)
    {
        SetPressedOnHandle(false);
    }
}

bool UGizmoVisualComponent::IntersectRayAxis(const FRay& Ray, FVector AxisEnd, float AxisScale, float& OutRayT)
{
	FVector AxisStart = GetWorldLocation();
	FVector RayOrigin = Ray.Origin;
	FVector RayDirection = Ray.Direction;

	FVector AxisVector = AxisEnd - AxisStart;
	FVector DiffOrigin = RayOrigin - AxisStart;

	float RayDirDotRayDir = RayDirection.X * RayDirection.X + RayDirection.Y * RayDirection.Y + RayDirection.Z * RayDirection.Z;
	float RayDirDotAxis = RayDirection.X * AxisVector.X + RayDirection.Y * AxisVector.Y + RayDirection.Z * AxisVector.Z;
	float AxisDotAxis = AxisVector.X * AxisVector.X + AxisVector.Y * AxisVector.Y + AxisVector.Z * AxisVector.Z;
	float RayDirDotDiff = RayDirection.X * DiffOrigin.X + RayDirection.Y * DiffOrigin.Y + RayDirection.Z * DiffOrigin.Z;
	float AxisDotDiff = AxisVector.X * DiffOrigin.X + AxisVector.Y * DiffOrigin.Y + AxisVector.Z * DiffOrigin.Z;

	float Denominator = (RayDirDotRayDir * AxisDotAxis) - (RayDirDotAxis * RayDirDotAxis);

	float RayT;
	float AxisS;

	if (Denominator < 1e-6f)
	{
		RayT = 0.0f;
		AxisS = (AxisDotAxis > 0.0f) ? (AxisDotDiff / AxisDotAxis) : 0.0f;
	}
	else
	{
		RayT = (RayDirDotAxis * AxisDotDiff - AxisDotAxis * RayDirDotDiff) / Denominator;
		AxisS = (RayDirDotRayDir * AxisDotDiff - RayDirDotAxis * RayDirDotDiff) / Denominator;
	}

	if (RayT < 0.0f) RayT = 0.0f;

	if (AxisS < 0.0f) AxisS = 0.0f;
	else if (AxisS > 1.0f) AxisS = 1.0f;

	FVector ClosestPointOnRay = RayOrigin + (RayDirection * RayT);
	FVector ClosestPointOnAxis = AxisStart + (AxisVector * AxisS);

	FVector DistanceVector = ClosestPointOnRay - ClosestPointOnAxis;
	float DistanceSquared = (DistanceVector.X * DistanceVector.X) +
		(DistanceVector.Y * DistanceVector.Y) +
		(DistanceVector.Z * DistanceVector.Z);

	//기즈모 픽킹에 원기둥 크기를 반영합니다.
	float ClickThreshold = Radius * AxisScale;
	constexpr float StemRadius = 0.06f;
	ClickThreshold = StemRadius * AxisScale;
	float ClickThresholdSquared = ClickThreshold * ClickThreshold;

	if (DistanceSquared < ClickThresholdSquared)
	{
		OutRayT = RayT;
		return true;
	}

	return false;
}

bool UGizmoVisualComponent::IntersectRayRotationHandle(const FRay& Ray, int32 Axis, float& OutRayT) const
{
	const FVector AxisVector = GetVectorForAxis(Axis).Normalized();
	const float Scale = (Axis == 0) ? GetWorldScale().X : (Axis == 1 ? GetWorldScale().Y : GetWorldScale().Z);
	const float RingRadius = AxisLength * Scale;
	const float RingThickness = Radius * Scale * 1.75f;

	const float Denom = Ray.Direction.Dot(AxisVector);
	if (std::abs(Denom) < 1e-6f)
	{
		return false;
	}

	const float RayT = (GetWorldLocation() - Ray.Origin).Dot(AxisVector) / Denom;
	if (RayT <= 0.0f)
	{
		return false;
	}

	const FVector HitPoint = Ray.Origin + Ray.Direction * RayT;
	const FVector Radial = HitPoint - GetWorldLocation();
	const FVector Planar = Radial - AxisVector * Radial.Dot(AxisVector);
	const float DistanceToRing = std::abs(Planar.Length() - RingRadius);
	if (DistanceToRing <= RingThickness)
	{
		OutRayT = RayT;
		return true;
	}

	return false;
}

bool UGizmoVisualComponent::IntersectRayCenterHandle(const FRay& Ray, float& OutRayT) const
{
	const FVector Center = GetWorldLocation();
	const float CenterScale = GetWorldScale().X;
	const float CenterRadius = 0.14f * CenterScale;

	const FVector ToCenter = Center - Ray.Origin;
	const float Projection = ToCenter.Dot(Ray.Direction);
	if (Projection < 0.0f)
	{
		return false;
	}

	const FVector ClosestPoint = Ray.Origin + Ray.Direction * Projection;
	const float DistanceSq = (ClosestPoint - Center).Dot(ClosestPoint - Center);
	const float RadiusSq = CenterRadius * CenterRadius;
	if (DistanceSq > RadiusSq)
	{
		return false;
	}

	const float Offset = std::sqrt((std::max)(0.0f, RadiusSq - DistanceSq));
	OutRayT = Projection - Offset;
	if (OutRayT < 0.0f)
	{
		OutRayT = Projection;
	}
	return true;
}

bool UGizmoVisualComponent::LineTraceComponent(const FRay& Ray, FRayHitResult& OutHitResult)
{
	OutHitResult = {};
	if (!IsVisible() || CurMode == EGizmoMode::Select || !HasVisualTarget() || !MeshData || MeshData->Indices.empty())
	{
		if (!IsHolding())
		{
			SelectedAxis = -1;
		}
		return false;
	}

	float BestRayT = FLT_MAX;
	int32 BestAxis = -1;
	const FVector GizmoLocation = GetWorldLocation();

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if ((AxisMask & (1u << Axis)) == 0)
		{
			continue;
		}

		float RayT = 0.0f;
		bool bAxisHit = false;
		if (CurMode == EGizmoMode::Rotate)
		{
			bAxisHit = IntersectRayRotationHandle(Ray, Axis, RayT);
		}
		else
		{
			const FVector AxisDir = GetVectorForAxis(Axis).Normalized();
			const float AxisScale = (Axis == 0) ? GetWorldScale().X : (Axis == 1 ? GetWorldScale().Y : GetWorldScale().Z);
			const FVector AxisEnd = GizmoLocation + AxisDir * AxisLength * AxisScale;
			bAxisHit = IntersectRayAxis(Ray, AxisEnd, AxisScale, RayT);
		}

		if (bAxisHit && RayT < BestRayT)
		{
			BestRayT = RayT;
			BestAxis = Axis;
		}
	}

	if (CurMode == EGizmoMode::Translate)
	{
		float CenterRayT = 0.0f;
		if (IntersectRayCenterHandle(Ray, CenterRayT) && CenterRayT < BestRayT)
		{
			BestRayT = CenterRayT;
			BestAxis = 3;
		}
	}

	if (BestAxis >= 0)
	{
		OutHitResult.bHit = true;
		OutHitResult.Distance = BestRayT;
		OutHitResult.HitComponent = this;
		if (!IsHolding())
		{
			SelectedAxis = BestAxis;
		}
		return true;
	}

	if (!IsHolding())
	{
		SelectedAxis = -1;
	}
	return false;
}


FVector UGizmoVisualComponent::GetVectorForAxis(int32 Axis) const
{
	switch (Axis)
	{
	case 0:
		return GetForwardVector();
	case 1:
		return GetRightVector();
	case 2:
		return GetUpVector();
	default:
		return FVector(0.f, 0.f, 0.f);
	}
}

void UGizmoVisualComponent::UpdateHoveredAxis(int Index)
{
	if (Index < 0)
	{
		if (IsHolding() == false) SelectedAxis = -1;
	}
	else
	{
		if (IsHolding() == false)
		{
			uint32 VertexIndex = MeshData->Indices[Index];
			uint32 HitAxis = MeshData->Vertices[VertexIndex].SubID;

			if (HitAxis == 3 || (AxisMask & (1u << HitAxis)))
			{
				SelectedAxis = HitAxis;
			}
			else
			{
				SelectedAxis = -1;
			}
		}
	}
}

void UGizmoVisualComponent::UpdateGizmoMode(EGizmoMode NewMode)
{
	CurMode = NewMode;
	UpdateVisualTransform();
}

void UGizmoVisualComponent::SetGizmoWorldTransform(const FTransform& InWorldTransform)
{
	VisualWorldTransform = InWorldTransform;
	bHasVisualTarget = true;
	UpdateVisualTransform();
}

void UGizmoVisualComponent::ClearGizmoWorldTransform()
{
    bHasVisualTarget = false;
    SetVisibility(false);
    ResetVisualInteractionState();
}

void UGizmoVisualComponent::UpdateVisualTransform()
{
    if (bHasVisualTarget)
    {
        ApplyGizmoWorldTransform(VisualWorldTransform);
        return;
    }

    SetVisibility(false);
    ResetVisualInteractionState();
}

void UGizmoVisualComponent::ApplyGizmoWorldTransform(const FTransform& InWorldTransform)
{
	if (CurMode == EGizmoMode::Select)
	{
		SetVisibility(false);
		return;
	}
	else
	{
		SetVisibility(true);
	}

	const FVector DesiredLocation = InWorldTransform.GetLocation();
	
	FRotator DesiredRotation = FRotator();
	if (CurMode == EGizmoMode::Scale || !bIsWorldSpace)
	{
		DesiredRotation = InWorldTransform.ToMatrix().ToRotator();
	}

	const FMeshData* DesiredMeshData = nullptr;

	switch (CurMode)
	{
	case EGizmoMode::Scale:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::ScaleGizmo);
		break;

	case EGizmoMode::Rotate:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::RotGizmo);
		break;

	case EGizmoMode::Translate:
		DesiredMeshData = &FMeshBufferManager::Get().GetMeshData(EMeshShape::TransGizmo);
		break;

	default:
		break;
	}

	if (FVector::DistSquared(GetWorldLocation(), DesiredLocation) > FMath::Epsilon * FMath::Epsilon)
	{
		SetWorldLocation(DesiredLocation);
	}

	if (GetRelativeRotation() != DesiredRotation)
	{
		SetRelativeRotation(DesiredRotation);
	}

	if (MeshData != DesiredMeshData && DesiredMeshData)
	{
		MeshData = DesiredMeshData;
		MarkRenderStateDirty();
	}
}

float UGizmoVisualComponent::ComputeScreenSpaceScale(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth) const
{
	float NewScale;
	if (bIsOrtho)
	{
		NewScale = OrthoWidth * GizmoScreenScale;
	}
	else
	{
		float Distance = FVector::Distance(CameraLocation, GetWorldLocation());
		NewScale = Distance * GizmoScreenScale;
	}
	return (NewScale < 0.01f) ? 0.01f : NewScale;
}

void UGizmoVisualComponent::ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth)
{
	float NewScale = ComputeScreenSpaceScale(CameraLocation, bIsOrtho, OrthoWidth);
	SetRelativeScale(FVector(NewScale, NewScale, NewScale));
}

void UGizmoVisualComponent::SetWorldSpace(bool bWorldSpace)
{
	bIsWorldSpace = bWorldSpace;
	UpdateVisualTransform();
}

uint32 UGizmoVisualComponent::ComputeAxisMask(ELevelViewportType ViewportType, EGizmoMode Mode)
{
	constexpr uint32 AllAxes = 0x7;
	uint32 ViewAxis = AllAxes;

	switch (ViewportType)
	{
	case ELevelViewportType::Top:
	case ELevelViewportType::Bottom:
		ViewAxis = 0x4; break; // Z
	case ELevelViewportType::Front:
	case ELevelViewportType::Back:
		ViewAxis = 0x1; break; // X
	case ELevelViewportType::Left:
	case ELevelViewportType::Right:
		ViewAxis = 0x2; break; // Y
	default: break;
	}

	if (ViewAxis == AllAxes)
		return AllAxes;

	if (Mode == EGizmoMode::Rotate)
		return ViewAxis;            // Rotate: 시선 축만

	return AllAxes & ~ViewAxis;     // Translate/Scale: 시선 축 제외
}

void UGizmoVisualComponent::Deactivate()
{
    ResetVisualInteractionState();
    ClearGizmoWorldTransform();
}

void UGizmoVisualComponent::ResetVisualInteractionState()
{
    bIsHolding = false;
    bPressedOnHandle = false;
    SelectedAxis = -1;
}

FMeshBuffer* UGizmoVisualComponent::GetMeshBuffer() const
{
	EMeshShape Shape = EMeshShape::TransGizmo;
	switch (CurMode)
	{
	case EGizmoMode::Translate:
		break;
	case EGizmoMode::Rotate:
		Shape = EMeshShape::RotGizmo;
		break;
	case EGizmoMode::Scale:
		Shape = EMeshShape::ScaleGizmo;
		break;
	case EGizmoMode::Select:
		break;
	}
	return &FMeshBufferManager::Get().GetMeshBuffer(Shape);
}
