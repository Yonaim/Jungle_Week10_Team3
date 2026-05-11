#include "Common/Gizmo/GizmoManager.h"

#include "Component/GizmoComponent.h"

void FGizmoManager::SetTarget(std::shared_ptr<ITransformProxy> InTarget)
{
    Target = InTarget;
    bDragging = false;
    SyncVisualFromTarget();
}

void FGizmoManager::ClearTarget()
{
    if (bDragging && Target && Target->IsValid())
    {
        Target->EndTransform();
    }

    Target.reset();
    bDragging = false;
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
    SyncVisualFromTarget();
}

void FGizmoManager::SetSpace(EGizmoSpace InSpace)
{
    Space = InSpace;
    SyncVisualFromTarget();
}

void FGizmoManager::SyncVisualFromTarget()
{
    if (!VisualComponent)
    {
        return;
    }

    if (!Target || !Target->IsValid())
    {
        VisualComponent->ClearGizmoWorldTransform();
        return;
    }

    VisualComponent->UpdateGizmoMode(Mode);
    VisualComponent->SetGizmoSpace(Space);
    VisualComponent->SetGizmoWorldTransform(Target->GetWorldTransform());
}
