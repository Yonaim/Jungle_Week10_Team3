#pragma once

#include "Component/Gizmo/GizmoTypes.h"
#include "Component/Gizmo/TransformProxy.h"

#include <memory>

class UGizmoComponent;

// Editor 공용 기즈모 컨트롤러.
// - 실제 조작 대상은 ITransformProxy로 추상화한다.
// - 화면에 보이는 3D 기즈모는 UGizmoComponent가 담당한다.
// - 이 단계에서는 렌더 동기화(1~16번)까지만 담당하고, 피킹/드래그는 후속 단계에서 연결한다.
class FGizmoManager
{
public:
    void SetTarget(std::shared_ptr<ITransformProxy> InTarget);
    void ClearTarget();

    bool HasValidTarget() const;

    void SetVisualComponent(UGizmoComponent* InComponent);
    UGizmoComponent* GetVisualComponent() const { return VisualComponent; }

    void       SetMode(EGizmoMode InMode);
    EGizmoMode GetMode() const { return Mode; }

    void        SetSpace(EGizmoSpace InSpace);
    EGizmoSpace GetSpace() const { return Space; }

    bool IsDragging() const { return bDragging; }

    void SyncVisualFromTarget();

private:
    std::shared_ptr<ITransformProxy> Target;
    UGizmoComponent* VisualComponent = nullptr;

    EGizmoMode  Mode = EGizmoMode::Translate;
    EGizmoSpace Space = EGizmoSpace::Local;

    bool bDragging = false;
};
