#include "Common/Gizmo/GizmoManager.h"

#include "Component/GizmoVisualComponent.h"
#include "Collision/RayUtils.h"
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

void FGizmoManager::SetVisualComponent(UGizmoVisualComponent* InComponent)
{
    if (VisualComponent == InComponent)
    {
        SyncVisualFromTarget();
        return;
    }

    if (VisualComponent)
    {
        VisualComponent->ResetVisualInteractionState();
    }

    VisualComponent = InComponent;
    SyncVisualFromTarget();
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

void FGizmoManager::ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth)
{
    if (VisualComponent && HasValidTarget())
    {
        VisualComponent->ApplyScreenSpaceScaling(CameraLocation, bIsOrtho, OrthoWidth);
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

bool FGizmoManager::HitTest(const FRay& Ray, FRayHitResult& OutHitResult)
{
    if (!CanInteract())
    {
        return false;
    }
    return FRayUtils::RaycastComponent(VisualComponent, Ray, OutHitResult);
}

bool FGizmoManager::BeginDrag(const FRay& Ray)
{
    if (!CanInteract())
    {
        return false;
    }

    FRayHitResult HitResult{};
    if (!HitTest(Ray, HitResult))
    {
        VisualComponent->SetPressedOnHandle(false);
        return false;
    }

    ActiveAxis = VisualComponent->GetSelectedAxis();
    if (ActiveAxis < 0)
    {
        VisualComponent->SetPressedOnHandle(false);
        return false;
    }

    Target->BeginTransform();
    DragStartTransform = Target->GetWorldTransform();
    LastIntersectionLocation = FVector::ZeroVector;
    bFirstDragUpdate = true;
    bDragging = true;
    ResetSnapAccumulation();

    VisualComponent->SetPressedOnHandle(true);
    VisualComponent->SetHolding(true);
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

FVector FGizmoManager::GetAxisVector(int32 Axis) const
{
    if (!VisualComponent)
    {
        return FVector::ZeroVector;
    }
    return VisualComponent->GetVectorForAxis(Axis).Normalized();
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
    const FVector CenterToLast = (LastIntersectionLocation - Center).Normalized();
    const FVector CenterToCurrent = (CurrentIntersection - Center).Normalized();

    const float DotProduct = Clamp(CenterToLast.Dot(CenterToCurrent), -1.0f, 1.0f);
    const float AngleRadians = std::acos(DotProduct);
    const FVector CrossProduct = CenterToLast.Cross(CenterToCurrent);
    const float Sign = (CrossProduct.Dot(AxisVector) >= 0.0f) ? 1.0f : -1.0f;

    FTransform NewTransform = Target->GetWorldTransform();
    const float DeltaAngle = ApplySnapToDragAmount(Sign * AngleRadians);
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
