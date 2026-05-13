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
    if (Space == InSpace)
    {
        SyncVisualFromTarget();
        return;
    }

    // Space changes redefine the visual axes and drag planes.  Do not allow a
    // toolbar/shortcut update in the middle of a drag to silently switch the
    // math basis while the old handle is still active.
    if (bDragging)
    {
        CancelDrag();
    }

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

    FTransform VisualTransform = Target->GetWorldTransform();

    if (bDragging)
    {
        // 드래그 중에는 기즈모의 기준 축이 변하면 안 된다.
        // Target transform은 매 프레임 갱신되지만, Local/Scale/Rotate 축까지 현재 transform에서
        // 다시 뽑으면 조작 중 링/축이 따라 돌면서 delta 계산과 visual 기준이 계속 바뀐다.
        // 따라서 위치는 현재 Target을 따라가되, 축을 결정하는 Rotation/Scale 기준은
        // 드래그 시작 시점의 transform으로 고정한다.
        VisualTransform.Rotation = DragStartTransform.Rotation;
        VisualTransform.Scale = DragStartTransform.Scale;
    }

    VisualComponent->SetGizmoWorldTransform(VisualTransform);
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
    if (!CanInteract() || !HitResult.bHit || HitResult.Axis < 0 || (HitResult.Axis == 3 && Mode != EGizmoMode::Translate))
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
    DragMode = Mode;
    DragSpace = Space;
    DragOriginLocation = DragStartTransform.GetLocation();

    if (ActiveAxis >= 0 && ActiveAxis <= 2)
    {
        DragAxisVector = GetAxisVectorFromTransform(DragStartTransform, ActiveAxis, DragMode, DragSpace).Normalized();
        if (DragAxisVector.IsNearlyZero())
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
    }
    else
    {
        DragAxisVector = FVector::ZeroVector;
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

    if (!std::isfinite(Ray.Origin.X) || !std::isfinite(Ray.Origin.Y) || !std::isfinite(Ray.Origin.Z) ||
        !std::isfinite(Ray.Direction.X) || !std::isfinite(Ray.Direction.Y) || !std::isfinite(Ray.Direction.Z) ||
        Ray.Direction.IsNearlyZero())
    {
        bFirstDragUpdate = true;
        return;
    }

    if (DragMode == EGizmoMode::Rotate)
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
    return GetAxisVectorFromTransform(Transform, Axis, Mode, Space);
}

FVector FGizmoManager::GetAxisVectorFromTransform(const FTransform& Transform, int32 Axis, EGizmoMode InMode, EGizmoSpace InSpace) const
{
    // 실제 조작 축은 visual component의 현재 relative matrix가 아니라
    // mode/space와 대상 transform에서 계산한다. Render/HitProxy/Drag가
    // 같은 정책을 쓰지 않으면 "보이는 축"과 "움직이는 축"이 어긋난다.
    //
    // Scale은 non-uniform scale + 임의 회전 상태에서 world-axis scale을
    // FTransform(Location/Rotation/Scale)만으로 정확히 표현할 수 없다.
    // shear가 필요하기 때문이다. 따라서 Scale은 항상 Local axis 기준으로
    // 표시/계산한다. UI도 effective space를 Local로 맞추는 것이 맞다.
    const bool bUseLocalAxes = (InMode == EGizmoMode::Scale || InSpace == EGizmoSpace::Local);
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
    if (AxisVector.IsNearlyZero())
    {
        return false;
    }

    const FVector PlaneOrigin = bDragging ? DragOriginLocation : VisualComponent->GetWorldLocation();
    const FVector PlaneNormal = AxisVector.Cross(Ray.Direction);
    if (PlaneNormal.Dot(PlaneNormal) <= 1.0e-8f)
    {
        return false;
    }

    const FVector ProjectDir = PlaneNormal.Cross(AxisVector);
    const float Denom = Ray.Direction.Dot(ProjectDir);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const float DistanceToPlane = (PlaneOrigin - Ray.Origin).Dot(ProjectDir) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return std::isfinite(OutPoint.X) && std::isfinite(OutPoint.Y) && std::isfinite(OutPoint.Z);
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

    if (DragPlaneNormal.IsNearlyZero())
    {
        return false;
    }

    const float Denom = Ray.Direction.Dot(DragPlaneNormal);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const FVector PlaneOrigin = bDragging ? DragOriginLocation : VisualComponent->GetWorldLocation();
    const float DistanceToPlane = (PlaneOrigin - Ray.Origin).Dot(DragPlaneNormal) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return std::isfinite(OutPoint.X) && std::isfinite(OutPoint.Y) && std::isfinite(OutPoint.Z);
}

bool FGizmoManager::ComputeAngularIntersection(const FRay& Ray, FVector& OutPoint)
{
    if (!VisualComponent || ActiveAxis < 0 || ActiveAxis > 2)
    {
        return false;
    }

    const FVector AxisVector = GetAxisVector(ActiveAxis);
    if (AxisVector.IsNearlyZero())
    {
        return false;
    }

    const float Denom = Ray.Direction.Dot(AxisVector);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const FVector PlaneOrigin = bDragging ? DragOriginLocation : VisualComponent->GetWorldLocation();
    const float DistanceToPlane = (PlaneOrigin - Ray.Origin).Dot(AxisVector) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return std::isfinite(OutPoint.X) && std::isfinite(OutPoint.Y) && std::isfinite(OutPoint.Z);
}


float FGizmoManager::ApplySnapToDragAmount(float DragAmount)
{
    bool bSnapEnabled = false;
    float SnapSize = 0.0f;

    const EGizmoMode SnapMode = bDragging ? DragMode : Mode;
    switch (SnapMode)
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
        // Ray/plane degeneracy can happen when the cursor leaves the viewport
        // or the ray becomes almost parallel to the active axis.  Re-anchor on
        // the next valid sample instead of applying a huge delta from a stale
        // intersection.
        bFirstDragUpdate = true;
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
        if (DragMode == EGizmoMode::Scale)
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
        bFirstDragUpdate = true;
        return;
    }

    if (bFirstDragUpdate)
    {
        LastIntersectionLocation = CurrentIntersection;
        bFirstDragUpdate = false;
        return;
    }

    const FVector AxisVector = GetAxisVector(ActiveAxis);
    const FVector Center = bDragging ? DragOriginLocation : VisualComponent->GetWorldLocation();
    FVector CenterToLast = LastIntersectionLocation - Center;
    FVector CenterToCurrent = CurrentIntersection - Center;
    if (CenterToLast.Dot(CenterToLast) <= 1.0e-8f || CenterToCurrent.Dot(CenterToCurrent) <= 1.0e-8f)
    {
        // Do not measure the next angle from a near-center vector.
        // That vector has an unstable direction and causes sudden rotation flips.
        bFirstDragUpdate = true;
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
