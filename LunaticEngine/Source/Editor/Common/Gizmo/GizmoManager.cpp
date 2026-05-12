#include "Common/Gizmo/GizmoManager.h"

#include "Component/GizmoComponent.h"
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
    SyncVisualFromTarget();
}

void FGizmoManager::ClearTarget()
{
    if (bDragging)
    {
        EndDrag();
    }

    Target.reset();
    SyncVisualFromTarget();
}

bool FGizmoManager::HasValidTarget() const
{
    return Target && Target->IsValid();
}

void FGizmoManager::SetVisualComponent(UGizmoComponent* InComponent)
{
    VisualComponent = InComponent;
    SyncVisualFromTarget();
}

void FGizmoManager::SetMode(EGizmoMode InMode)
{
    Mode = InMode;
    if (bDragging)
    {
        return;
    }

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
    if (bDragging)
    {
        return;
    }

    if (VisualComponent)
    {
        VisualComponent->SetGizmoSpace(Space);
    }
    SyncVisualFromTarget();
}

void FGizmoManager::SyncVisualFromTarget()
{
    if (bDragging)
    {
        return;
    }

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

bool FGizmoManager::HitTest(const FRay& Ray, FRayHitResult& OutHitResult)
{
    if (!VisualComponent || !HasValidTarget())
    {
        return false;
    }
    return FRayUtils::RaycastComponent(VisualComponent, Ray, OutHitResult);
}

bool FGizmoManager::BeginDrag(const FRay& Ray)
{
    if (!VisualComponent || !HasValidTarget())
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
    DragMode = Mode;
    DragSpace = Space;
    DragStartTransform = Target->GetWorldTransform();
    DragStartLocalTransform = Target->GetLocalTransform();
    DragOriginLocation = DragStartTransform.GetLocation();
    DragAxisVector = (ActiveAxis >= 0 && ActiveAxis <= 2) ? GetAxisVector(ActiveAxis) : FVector::ZeroVector;
    DragStartIntersectionLocation = FVector::ZeroVector;
    LastIntersectionLocation = FVector::ZeroVector;
    bFirstDragUpdate = true;

    FVector InitialIntersection;
    const bool bHasInitialIntersection =
        (DragMode == EGizmoMode::Rotate) ? ComputeAngularIntersection(Ray, InitialIntersection) :
        (ActiveAxis == 3)                ? ComputePlanarIntersection(Ray, InitialIntersection) :
                                           ComputeLinearIntersection(Ray, InitialIntersection);
    if (bHasInitialIntersection)
    {
        DragStartIntersectionLocation = InitialIntersection;
        LastIntersectionLocation = InitialIntersection;
        bFirstDragUpdate = false;
    }

    bDragging = true;

    VisualComponent->SetPressedOnHandle(true);
    VisualComponent->SetHolding(true);
    return true;
}

void FGizmoManager::UpdateDrag(const FRay& Ray)
{
    if (!bDragging || !VisualComponent || !HasValidTarget())
    {
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

    // Keep the visual transform on the drag-start basis while applying pose edits.
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
    DragMode = Mode;
    DragSpace = Space;
    DragStartLocalTransform = FTransform();
    DragOriginLocation = FVector::ZeroVector;
    DragAxisVector = FVector::ZeroVector;
    DragStartIntersectionLocation = FVector::ZeroVector;

    if (VisualComponent)
    {
        VisualComponent->DragEnd();
    }
    SyncVisualFromTarget();
}

void FGizmoManager::CancelDrag()
{
    if (bDragging && Target && Target->IsValid())
    {
        if (DragMode == EGizmoMode::Scale)
        {
            Target->SetLocalTransform(DragStartLocalTransform);
        }
        else
        {
            Target->SetWorldTransform(DragStartTransform);
        }
    }
    EndDrag();
}

void FGizmoManager::SetTargetWorldLocation(const FVector& NewLocation)
{
    if (!HasValidTarget())
    {
        return;
    }
    Target->SetWorldLocation(NewLocation);
    if (!bDragging)
    {
        SyncVisualFromTarget();
    }
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

    const FVector AxisVector = DragAxisVector;
    const FVector PlaneNormal = AxisVector.Cross(Ray.Direction);
    const FVector ProjectDir = PlaneNormal.Cross(AxisVector);
    const float Denom = Ray.Direction.Dot(ProjectDir);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const float DistanceToPlane = (DragOriginLocation - Ray.Origin).Dot(ProjectDir) / Denom;
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

    const float DistanceToPlane = (DragOriginLocation - Ray.Origin).Dot(DragPlaneNormal) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return true;
}

bool FGizmoManager::ComputeAngularIntersection(const FRay& Ray, FVector& OutPoint)
{
    if (!VisualComponent || ActiveAxis < 0 || ActiveAxis > 2)
    {
        return false;
    }

    const FVector AxisVector = DragAxisVector;
    const float Denom = Ray.Direction.Dot(AxisVector);
    if (std::abs(Denom) < 1e-6f)
    {
        return false;
    }

    const float DistanceToPlane = (DragOriginLocation - Ray.Origin).Dot(AxisVector) / Denom;
    OutPoint = Ray.Origin + Ray.Direction * DistanceToPlane;
    return true;
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
        DragStartIntersectionLocation = CurrentIntersection;
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
        const FVector AxisVector = DragAxisVector;
        const float DragAmount = FullDelta.Dot(AxisVector);
        if (DragMode == EGizmoMode::Scale)
        {
            constexpr float BoneScaleDragSensitivity = 0.25f;

            FTransform LocalTransform = Target->GetLocalTransform();
            FVector NewScale = LocalTransform.Scale;
            const float ScaleDelta = DragAmount * BoneScaleDragSensitivity;
            if (ActiveAxis == 0) NewScale.X = (std::max)(0.001f, NewScale.X + ScaleDelta);
            if (ActiveAxis == 1) NewScale.Y = (std::max)(0.001f, NewScale.Y + ScaleDelta);
            if (ActiveAxis == 2) NewScale.Z = (std::max)(0.001f, NewScale.Z + ScaleDelta);
            LocalTransform.Scale = NewScale;
            Target->SetLocalTransform(LocalTransform);
        }
        else
        {
            NewTransform.SetLocation(NewTransform.GetLocation() + AxisVector * DragAmount);
            Target->SetWorldLocation(NewTransform.GetLocation());
        }
    }

    if (bPlanar)
    {
        Target->SetWorldLocation(NewTransform.GetLocation());
    }

    if (VisualComponent)
    {
        if (DragMode == EGizmoMode::Scale)
        {
            VisualComponent->SetGizmoWorldTransform(DragStartTransform);
        }
        else
        {
            FTransform VisualTransform = DragStartTransform;
            VisualTransform.SetLocation(NewTransform.GetLocation());
            VisualComponent->SetGizmoWorldTransform(VisualTransform);
        }
    }
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
        DragStartIntersectionLocation = CurrentIntersection;
        LastIntersectionLocation = CurrentIntersection;
        bFirstDragUpdate = false;
        return;
    }

    const FVector AxisVector = DragAxisVector;
    const FVector Center = DragOriginLocation;
    const FVector StartOffset = DragStartIntersectionLocation - Center;
    const FVector CurrentOffset = CurrentIntersection - Center;
    if (StartOffset.Dot(StartOffset) <= FMath::Epsilon ||
        CurrentOffset.Dot(CurrentOffset) <= FMath::Epsilon)
    {
        return;
    }

    const FVector CenterToLast = StartOffset.Normalized();
    const FVector CenterToCurrent = CurrentOffset.Normalized();

    const float DotProduct = Clamp(CenterToLast.Dot(CenterToCurrent), -1.0f, 1.0f);
    const float AngleRadians = std::acos(DotProduct);
    constexpr float MinAngularDragRadians = 0.001f;
    if (AngleRadians <= MinAngularDragRadians)
    {
        return;
    }

    const FVector CrossProduct = CenterToLast.Cross(CenterToCurrent);
    const float Sign = (CrossProduct.Dot(AxisVector) >= 0.0f) ? 1.0f : -1.0f;

    FTransform NewTransform = DragStartTransform;
    const FQuat DeltaQuat = FQuat::FromAxisAngle(AxisVector, Sign * AngleRadians);
    NewTransform.Rotation = (DeltaQuat * DragStartTransform.Rotation).GetNormalized();
    Target->SetWorldRotation(NewTransform.Rotation);

    if (VisualComponent)
    {
        VisualComponent->SetGizmoWorldTransform(DragStartTransform);
    }

    LastIntersectionLocation = CurrentIntersection;
}
