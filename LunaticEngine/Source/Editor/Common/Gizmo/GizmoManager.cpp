#include "Common/Gizmo/GizmoManager.h"

void FGizmoManager::SetTarget(std::shared_ptr<ITransformProxy> InTarget)
{
    Target = InTarget;
    bDragging = false;
    bHot = false;
}

void FGizmoManager::ClearTarget()
{
    if (bDragging && Target && Target->IsValid())
    {
        Target->EndTransform();
    }

    Target.reset();
    bDragging = false;
    bHot = false;
}

bool FGizmoManager::HasValidTarget() const { return Target && Target->IsValid(); }
