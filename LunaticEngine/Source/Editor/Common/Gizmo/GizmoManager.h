#pragma once

#include "Component/Gizmo/GizmoTypes.h"
#include "Component/Gizmo/TransformGizmoTarget.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
#include "Math/Transform.h"

#include <memory>

class UGizmoVisualComponent;

enum class EGizmoInteractionPolicy : uint8
{
    Interactive,
    VisualOnly
};

struct FGizmoTargetKey
{
    const void* Owner = nullptr;
    int32 SubIndex = -1;

    bool operator==(const FGizmoTargetKey& Other) const
    {
        return Owner == Other.Owner && SubIndex == Other.SubIndex;
    }

    bool operator!=(const FGizmoTargetKey& Other) const
    {
        return !(*this == Other);
    }
};

// Editor 공용 기즈모 컨트롤러.
// - 실제 조작 대상은 ITransformGizmoTarget로 추상화한다.
// - 화면에 보이는 3D 기즈모는 UGizmoVisualComponent가 담당한다.
// - mode/space/snap/피킹/드래그/타겟 Transform 반영은 Manager가 담당한다.
class FGizmoManager
{
public:
    void SetTarget(std::shared_ptr<ITransformGizmoTarget> InTarget);
    bool SetTargetIfChanged(const FGizmoTargetKey& InKey, std::shared_ptr<ITransformGizmoTarget> InTarget);
    void ClearTarget();

    bool HasValidTarget() const;
    std::shared_ptr<ITransformGizmoTarget> GetTarget() const { return Target; }

    void SetVisualComponent(UGizmoVisualComponent* InComponent);
    UGizmoVisualComponent* GetVisualComponent() const { return VisualComponent; }

    void SetInteractionPolicy(EGizmoInteractionPolicy InPolicy);
    EGizmoInteractionPolicy GetInteractionPolicy() const { return InteractionPolicy; }
    bool CanInteract() const;

    void       SetMode(EGizmoMode InMode);
    EGizmoMode GetMode() const { return Mode; }
    void       CycleMode();

    void        SetSpace(EGizmoSpace InSpace);
    EGizmoSpace GetSpace() const { return Space; }

    void SetSnapSettings(bool bTranslationEnabled, float InTranslationSnapSize,
                         bool bRotationEnabled, float InRotationSnapSizeDegrees,
                         bool bScaleEnabled, float InScaleSnapSize);

    bool IsDragging() const { return bDragging; }

    void SyncVisualFromTarget();
    void ApplyScreenSpaceScaling(const FVector& CameraLocation, bool bIsOrtho, float OrthoWidth);
    void SetAxisMask(uint32 InAxisMask);
    void ResetVisualInteractionState();

    bool HitTest(const FRay& Ray, FRayHitResult& OutHitResult);
    bool BeginDrag(const FRay& Ray);
    void UpdateDrag(const FRay& Ray);
    void EndDrag();
    void CancelDrag();

    void SetTargetWorldLocation(const FVector& NewLocation);

private:
    FVector GetAxisVector(int32 Axis) const;
    bool ComputeLinearIntersection(const FRay& Ray, FVector& OutPoint);
    bool ComputePlanarIntersection(const FRay& Ray, FVector& OutPoint);
    bool ComputeAngularIntersection(const FRay& Ray, FVector& OutPoint);
    float ApplySnapToDragAmount(float DragAmount);
    void ResetSnapAccumulation();
    void ApplyLinearDrag(const FRay& Ray);
    void ApplyAngularDrag(const FRay& Ray);

private:
    std::shared_ptr<ITransformGizmoTarget> Target;
    FGizmoTargetKey TargetKey{};
    bool bHasTargetKey = false;
    UGizmoVisualComponent* VisualComponent = nullptr;

    EGizmoInteractionPolicy InteractionPolicy = EGizmoInteractionPolicy::Interactive;
    EGizmoMode  Mode = EGizmoMode::Translate;
    EGizmoSpace Space = EGizmoSpace::Local;

    bool bDragging = false;
    bool bFirstDragUpdate = true;
    int32 ActiveAxis = -1;
    FTransform DragStartTransform;
    FVector LastIntersectionLocation;
    FVector DragPlaneNormal = FVector(0.0f, 0.0f, 1.0f);

    bool bTranslationSnapEnabled = false;
    bool bRotationSnapEnabled = false;
    bool bScaleSnapEnabled = false;
    float TranslationSnapSize = 10.0f;
    float RotationSnapSizeRadians = 0.261799395f;
    float ScaleSnapSize = 0.1f;
    float AccumulatedRawDragAmount = 0.0f;
    float LastAppliedSnappedDragAmount = 0.0f;
};
