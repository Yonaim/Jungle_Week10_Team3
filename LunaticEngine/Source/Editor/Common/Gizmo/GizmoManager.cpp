#include "PCH/LunaticPCH.h"
#include "Common/Gizmo/GizmoManager.h"

#include "Component/GizmoVisualComponent.h"
#include "Object/Object.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

void FGizmoManager::SetTarget(std::shared_ptr<ITransformGizmoTarget> InTarget)
{
    if (bDragging)
    {
        EndDrag();
    }

    Target = InTarget;
    bHasTargetKey = false;
    SyncVisualFromTarget();
}

bool FGizmoManager::SetTargetIfChanged(const FGizmoTargetKey& InKey, std::shared_ptr<ITransformGizmoTarget> InTarget)
{
    if (bHasTargetKey && TargetKey == InKey && HasValidTarget())
    {
        SyncVisualFromTarget();
        return false;
    }

    if (bDragging)
    {
        EndDrag();
    }

    TargetKey = InKey;
    bHasTargetKey = true;
    Target = InTarget;
    SyncVisualFromTarget();
    return true;
}

void FGizmoManager::ClearTarget()
{
    if (bDragging)
    {
        EndDrag();
    }

    Target.reset();
    bHasTargetKey = false;
    SyncVisualFromTarget();
}

bool FGizmoManager::HasValidTarget() const
{
    return Target && Target->IsValid();
}

void FGizmoManager::EnsureVisualComponent()
{
    if (VisualComponent)
    {
        SyncVisualFromTarget();
        return;
    }

    VisualComponent = UObjectManager::Get().CreateObject<UGizmoVisualComponent>();
    SyncVisualFromTarget();
}

void FGizmoManager::ReleaseVisualComponent()
{
    if (!VisualComponent)
    {
        RegisteredVisualScene = nullptr;
        return;
    }

    UnregisterVisualFromScene();
    VisualComponent->ResetVisualInteractionState();
    UObjectManager::Get().DestroyObject(VisualComponent);
    VisualComponent = nullptr;
}

void FGizmoManager::SetVisualWorldLocation(const FVector& InLocation)
{
    EnsureVisualComponent();
    if (VisualComponent)
    {
        VisualComponent->SetWorldLocation(InLocation);
    }
}

void FGizmoManager::SetVisualScene(FScene* InScene)
{
    EnsureVisualComponent();
    if (VisualComponent)
    {
        VisualComponent->SetScene(InScene);
    }
}

void FGizmoManager::ClearVisualScene()
{
    if (VisualComponent)
    {
        VisualComponent->SetScene(nullptr);
    }
}

void FGizmoManager::CreateVisualRenderState()
{
    EnsureVisualComponent();
    if (VisualComponent)
    {
        VisualComponent->CreateRenderState();
    }
}

void FGizmoManager::DestroyVisualRenderState()
{
    if (VisualComponent)
    {
        VisualComponent->DestroyRenderState();
    }
}

void FGizmoManager::RegisterVisualToScene(FScene* InScene)
{
    if (!InScene)
    {
        UnregisterVisualFromScene();
        return;
    }

    EnsureVisualComponent();

    // Mode 전환 시 UGizmoVisualComponent::MarkRenderStateDirty()가 호출되면
    // DestroyRenderState() -> CreateRenderState() 순서로 proxy가 재생성된다.
    // 이때 Actor 없이 독립 생성된 gizmo는 Scene 포인터가 반드시 다시 보장되어야 한다.
    // 따라서 같은 Scene이어도 SetVisualScene/CreateVisualRenderState를 idempotent하게 호출해
    // stale registration 상태를 회복할 수 있게 한다.
    if (RegisteredVisualScene != InScene)
    {
        UnregisterVisualFromScene();
        RegisteredVisualScene = InScene;
    }

    SetVisualScene(InScene);
    CreateVisualRenderState();
    SyncVisualFromTarget();
}

void FGizmoManager::UnregisterVisualFromScene()
{
    if (RegisteredVisualScene)
    {
        DestroyVisualRenderState();
    }
    ClearVisualScene();
    RegisteredVisualScene = nullptr;
}

void FGizmoManager::SetInteractionPolicy(EGizmoInteractionPolicy InPolicy)
{
    if (InteractionPolicy == InPolicy)
    {
        return;
    }

    InteractionPolicy = InPolicy;
    if (InteractionPolicy == EGizmoInteractionPolicy::VisualOnly)
    {
        CancelDrag();
        ResetVisualInteractionState();
    }
}

bool FGizmoManager::CanInteract() const
{
    return InteractionPolicy == EGizmoInteractionPolicy::Interactive && VisualComponent && HasValidTarget();
}

void FGizmoManager::SetMode(EGizmoMode InMode)
{
    if (Mode == InMode)
    {
        SyncVisualFromTarget();
        return;
    }

    // Mode change invalidates the currently selected handle and drag plane.
    // Keep this transition manager-owned so toolbar/shortcut paths cannot leave
    // UGizmoVisualComponent in a stale pressed/holding state.
    CancelDrag();
    ResetVisualInteractionState();

    Mode = InMode;
    if (VisualComponent)
    {
        VisualComponent->UpdateGizmoMode(Mode);
    }
    SyncVisualFromTarget();
}

void FGizmoManager::CycleMode()
{
    int NextInt = (static_cast<int>(Mode) + 1) % static_cast<int>(EGizmoMode::End);
    if (NextInt == 0)
    {
        NextInt = 1;
    }
    SetMode(static_cast<EGizmoMode>(NextInt));
}

void FGizmoManager::SetSpace(EGizmoSpace InSpace)
{
    Space = InSpace;
    if (VisualComponent)
    {
        VisualComponent->SetGizmoSpace(Space);
    }
    SyncVisualFromTarget();
}

void FGizmoManager::SetSnapSettings(bool bTranslationEnabled, float InTranslationSnapSize,
                                    bool bRotationEnabled, float InRotationSnapSizeDegrees,
                                    bool bScaleEnabled, float InScaleSnapSize)
{
    bTranslationSnapEnabled = bTranslationEnabled;
    TranslationSnapSize = (InTranslationSnapSize > FMath::Epsilon) ? InTranslationSnapSize : 10.0f;
    bRotationSnapEnabled = bRotationEnabled;
    RotationSnapSizeRadians = ((InRotationSnapSizeDegrees > FMath::Epsilon) ? InRotationSnapSizeDegrees : 15.0f) * DEG_TO_RAD;
    bScaleSnapEnabled = bScaleEnabled;
    ScaleSnapSize = (InScaleSnapSize > FMath::Epsilon) ? InScaleSnapSize : 0.1f;
}

void FGizmoManager::SyncVisualFromTarget()
{
    if (!VisualComponent)
    {
        return;
    }

    VisualComponent->UpdateGizmoMode(Mode);
    VisualComponent->SetGizmoSpace(Space);

    if (!Target || !Target->IsValid())
    {
        VisualComponent->ClearGizmoWorldTransform();
        return;
    }

    VisualComponent->SetGizmoWorldTransform(Target->GetWorldTransform());
}

void FGizmoManager::ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth, float ViewportHeight)
{
    if (VisualComponent && HasValidTarget())
    {
        VisualComponent->ApplyScreenSpaceScaling(CameraLocation, bIsOrtho, OrthoWidth, ViewportHeight);
    }
}

void FGizmoManager::SetAxisMask(uint32 InAxisMask)
{
    if (VisualComponent)
    {
        VisualComponent->SetAxisMask(InAxisMask);
    }
}

void FGizmoManager::ResetVisualInteractionState()
{
    if (VisualComponent)
    {
        VisualComponent->ResetVisualInteractionState();
    }
}

bool FGizmoManager::HitTestHitProxy(const FGizmoHitProxyContext& Context, FGizmoHitProxyResult& OutHitResult)
{
    OutHitResult.Reset();

    if (!CanInteract() || Mode == EGizmoMode::Select)
    {
        return false;
    }

    SyncVisualFromTarget();

    if (!VisualComponent || !VisualComponent->IsVisible() || !VisualComponent->HasVisualTarget())
    {
        return false;
    }

    if (Target && Target->IsValid())
    {
        const FVector TargetLocation = Target->GetWorldTransform().GetLocation();
        const FVector VisualLocation = VisualComponent->GetWorldLocation();
        const float AllowedErrorSq = 0.01f;
        if (FVector::DistSquared(TargetLocation, VisualLocation) > AllowedErrorSq)
        {
            VisualComponent->ClearGizmoWorldTransform();
            return false;
        }
    }

    if (!bDragging && VisualComponent->IsHolding())
    {
        VisualComponent->ResetVisualInteractionState();
    }

    if (!VisualComponent->HitProxyTest(Context, OutHitResult))
    {
        if (!VisualComponent->IsHolding())
        {
            VisualComponent->SetSelectedAxis(-1);
        }
        return false;
    }

    if (!VisualComponent->IsHolding())
    {
        VisualComponent->SetSelectedAxis(OutHitResult.Axis);
    }
    return true;
}

bool FGizmoManager::BeginDragFromHitProxy(const FGizmoHitProxyResult& HitResult)
{
    if (!CanInteract() || !HitResult.bHit || HitResult.Axis < 0)
    {
        if (VisualComponent)
        {
            VisualComponent->SetPressedOnHandle(false);
        }
        return false;
    }

    ActiveAxis = HitResult.Axis;
    if (VisualComponent)
    {
        VisualComponent->SetSelectedAxis(ActiveAxis);
    }

    Target->BeginTransform();
    DragStartTransform = Target->GetWorldTransform();
    DragAxisVector = GetAxisVectorFromTransform(DragStartTransform, ActiveAxis).Normalized();
    if (ActiveAxis >= 0 && ActiveAxis <= 2 && DragAxisVector.IsNearlyZero())
    {
        Target->EndTransform();
        ActiveAxis = -1;
        if (VisualComponent)
        {
            VisualComponent->SetPressedOnHandle(false);
            VisualComponent->SetHolding(false);
        }
        return false;
    }
    LastIntersectionLocation = FVector::ZeroVector;
    bFirstDragUpdate = true;
    bDragging = true;
    ResetSnapAccumulation();

    if (VisualComponent)
    {
        VisualComponent->SetPressedOnHandle(true);
        VisualComponent->SetHolding(true);
    }
    return true;
}

void FGizmoManager::UpdateDrag(const FRay& Ray)
{
    if (!bDragging || !CanInteract())
    {
        return;
    }

    if (Mode == EGizmoMode::Rotate)
    {
        ApplyAngularDrag(Ray);
    }
    else
    {
        ApplyLinearDrag(Ray);
    }

    SyncVisualFromTarget();
}

void FGizmoManager::EndDrag()
{
    if (bDragging && Target && Target->IsValid())
    {
        Target->EndTransform();
    }

    bDragging = false;
    bFirstDragUpdate = true;
    ActiveAxis = -1;
    ResetSnapAccumulation();

    ResetVisualInteractionState();
    SyncVisualFromTarget();
}

void FGizmoManager::CancelDrag()
{
    if (bDragging && Target && Target->IsValid())
    {
        Target->SetWorldTransform(DragStartTransform);
        Target->EndTransform();
    }

    bDragging = false;
    bFirstDragUpdate = true;
    ActiveAxis = -1;
    ResetSnapAccumulation();
    ResetVisualInteractionState();
    SyncVisualFromTarget();
}

void FGizmoManager::SetTargetWorldLocation(const FVector& NewLocation)
{
    if (!HasValidTarget())
    {
        return;
    }
    FTransform Transform = Target->GetWorldTransform();
    Transform.SetLocation(NewLocation);
    Target->SetWorldTransform(Transform);
    SyncVisualFromTarget();
}

FVector FGizmoManager::GetAxisVectorFromTransform(const FTransform& Transform, int32 Axis) const
{
    // 실제 조작 축은 visual component의 현재 matrix가 아니라 mode/space와 대상 transform에서 계산한다.
    // 이렇게 해야 Render/HitProxy/Drag가 동일한 축 정책을 쓰고, Euler/scale 라운드트립으로
    // 한 축만 뒤집히는 문제가 드래그 계산에 전파되지 않는다.
    const bool bUseLocalAxes = (Mode == EGizmoMode::Scale || Space == EGizmoSpace::Local);
    if (!bUseLocalAxes)
    {
        switch (Axis)
        {
        case 0: return FVector(1.0f, 0.0f, 0.0f);
        case 1: return FVector(0.0f, 1.0f, 0.0f);
        case 2: return FVector(0.0f, 0.0f, 1.0f);
        default: return FVector::ZeroVector;
        }
    }

    const FQuat Rotation = Transform.Rotation.GetNormalized();
    switch (Axis)
    {
    case 0: return Rotation.GetForwardVector().Normalized();
    case 1: return Rotation.GetRightVector().Normalized();
    case 2: return Rotation.GetUpVector().Normalized();
    default: return FVector::ZeroVector;
    }
}

FVector FGizmoManager::GetAxisVector(int32 Axis) const
{
    if (bDragging && Axis == ActiveAxis && Axis >= 0 && Axis <= 2)
    {
        return DragAxisVector.Normalized();
    }

    if (Target && Target->IsValid())
    {
        return GetAxisVectorFromTransform(Target->GetWorldTransform(), Axis).Normalized();
    }

    return FVector::ZeroVector;
}

bool FGizmoManager::ComputeLinearIntersection(const FRay& Ray, FVector& OutPoint)
{
    if (!VisualComponent || ActiveAxis < 0 || ActiveAxis > 2)
    {
        return false;
    }

    const FVector AxisVector = GetAxisVector(ActiveAxis);
    const FVector PlaneNormal = AxisVector.Cross(Ray.Direction);
    const FVector ProjectDir = PlaneNormal.Cross(AxisVector);
    const float Denom = Ray.Direction.Dot(ProjectDir);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const float DistanceToPlane = (VisualComponent->GetWorldLocation() - Ray.Origin).Dot(ProjectDir) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return true;
}

bool FGizmoManager::ComputePlanarIntersection(const FRay& Ray, FVector& OutPoint)
{
    if (!VisualComponent)
    {
        return false;
    }

    if (bFirstDragUpdate)
    {
        DragPlaneNormal = (Ray.Direction * -1.0f).Normalized();
    }

    const float Denom = Ray.Direction.Dot(DragPlaneNormal);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const float DistanceToPlane = (VisualComponent->GetWorldLocation() - Ray.Origin).Dot(DragPlaneNormal) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return true;
}

bool FGizmoManager::ComputeAngularIntersection(const FRay& Ray, FVector& OutPoint)
{
    if (!VisualComponent || ActiveAxis < 0 || ActiveAxis > 2)
    {
        return false;
    }

    const FVector AxisVector = GetAxisVector(ActiveAxis);
    const float Denom = Ray.Direction.Dot(AxisVector);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const float DistanceToPlane = (VisualComponent->GetWorldLocation() - Ray.Origin).Dot(AxisVector) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return true;
}


float FGizmoManager::ApplySnapToDragAmount(float DragAmount)
{
    bool bSnapEnabled = false;
    float SnapSize = 0.0f;

    switch (Mode)
    {
    case EGizmoMode::Translate:
        bSnapEnabled = bTranslationSnapEnabled;
        SnapSize = TranslationSnapSize;
        break;
    case EGizmoMode::Rotate:
        bSnapEnabled = bRotationSnapEnabled;
        SnapSize = RotationSnapSizeRadians;
        break;
    case EGizmoMode::Scale:
        bSnapEnabled = bScaleSnapEnabled;
        SnapSize = ScaleSnapSize;
        break;
    default:
        break;
    }

    if (!bSnapEnabled || SnapSize <= FMath::Epsilon)
    {
        return DragAmount;
    }

    AccumulatedRawDragAmount += DragAmount;
    const float SnappedTotal = std::floor((AccumulatedRawDragAmount / SnapSize) + 0.5f) * SnapSize;
    const float DeltaToApply = SnappedTotal - LastAppliedSnappedDragAmount;
    LastAppliedSnappedDragAmount = SnappedTotal;
    return DeltaToApply;
}

void FGizmoManager::ResetSnapAccumulation()
{
    AccumulatedRawDragAmount = 0.0f;
    LastAppliedSnappedDragAmount = 0.0f;
}

void FGizmoManager::ApplyLinearDrag(const FRay& Ray)
{
    FVector CurrentIntersection;
    const bool bPlanar = (ActiveAxis == 3);
    const bool bHit = bPlanar ? ComputePlanarIntersection(Ray, CurrentIntersection)
                              : ComputeLinearIntersection(Ray, CurrentIntersection);
    if (!bHit)
    {
        return;
    }

    if (bFirstDragUpdate)
    {
        LastIntersectionLocation = CurrentIntersection;
        bFirstDragUpdate = false;
        return;
    }

    const FVector FullDelta = CurrentIntersection - LastIntersectionLocation;
    if (FullDelta.Dot(FullDelta) <= FMath::Epsilon)
    {
        return;
    }

    FTransform NewTransform = Target->GetWorldTransform();
    if (bPlanar)
    {
        NewTransform.SetLocation(NewTransform.GetLocation() + FullDelta);
    }
    else
    {
        const FVector AxisVector = GetAxisVector(ActiveAxis);
        float DragAmount = FullDelta.Dot(AxisVector);
        DragAmount = ApplySnapToDragAmount(DragAmount);
        if (std::abs(DragAmount) <= FMath::Epsilon)
        {
            LastIntersectionLocation = CurrentIntersection;
            return;
        }
        if (Mode == EGizmoMode::Scale)
        {
            FVector NewScale = NewTransform.Scale;
            const float ScaleDelta = DragAmount;
            if (ActiveAxis == 0) NewScale.X = (std::max)(0.001f, NewScale.X + ScaleDelta);
            if (ActiveAxis == 1) NewScale.Y = (std::max)(0.001f, NewScale.Y + ScaleDelta);
            if (ActiveAxis == 2) NewScale.Z = (std::max)(0.001f, NewScale.Z + ScaleDelta);
            NewTransform.Scale = NewScale;
        }
        else
        {
            NewTransform.SetLocation(NewTransform.GetLocation() + AxisVector * DragAmount);
        }
    }

    Target->SetWorldTransform(NewTransform);
    LastIntersectionLocation = CurrentIntersection;
}

void FGizmoManager::ApplyAngularDrag(const FRay& Ray)
{
    FVector CurrentIntersection;
    if (!ComputeAngularIntersection(Ray, CurrentIntersection))
    {
        return;
    }

    if (bFirstDragUpdate)
    {
        LastIntersectionLocation = CurrentIntersection;
        bFirstDragUpdate = false;
        return;
    }

    const FVector AxisVector = GetAxisVector(ActiveAxis);
    const FVector Center = VisualComponent->GetWorldLocation();
    FVector CenterToLast = LastIntersectionLocation - Center;
    FVector CenterToCurrent = CurrentIntersection - Center;
    if (CenterToLast.Dot(CenterToLast) <= 1.0e-8f || CenterToCurrent.Dot(CenterToCurrent) <= 1.0e-8f)
    {
        LastIntersectionLocation = CurrentIntersection;
        return;
    }
    CenterToLast = CenterToLast.Normalized();
    CenterToCurrent = CenterToCurrent.Normalized();

    // acos(dot)+sign 방식은 0/180도 근처에서 부호가 튀기 쉽다.
    // atan2(signedSin, cos)를 쓰면 작은 각도, 큰 각도 모두 연속적으로 계산된다.
    const float SignedSin = AxisVector.Dot(CenterToLast.Cross(CenterToCurrent));
    const float CosAngle = Clamp(CenterToLast.Dot(CenterToCurrent), -1.0f, 1.0f);
    const float AngleRadians = std::atan2(SignedSin, CosAngle);

    FTransform NewTransform = Target->GetWorldTransform();
    const float DeltaAngle = ApplySnapToDragAmount(AngleRadians);
    if (std::abs(DeltaAngle) <= FMath::Epsilon)
    {
        LastIntersectionLocation = CurrentIntersection;
        return;
    }

    const FQuat DeltaQuat = FQuat::FromAxisAngle(AxisVector, DeltaAngle);
    NewTransform.Rotation = (DeltaQuat * NewTransform.Rotation).GetNormalized();
    Target->SetWorldTransform(NewTransform);

    LastIntersectionLocation = CurrentIntersection;
}
