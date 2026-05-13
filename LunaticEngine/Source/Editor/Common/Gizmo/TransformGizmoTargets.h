#pragma once

#include "Component/Gizmo/TransformGizmoTarget.h"
#include "Component/SceneComponent.h"

// SceneComponent를 기즈모 조작 대상으로 감싸는 공통 어댑터.
// Level Editor의 Actor/Component 선택과 Asset Preview의 PreviewComponent 선택 모두 이 경로를 쓸 수 있다.
class FSceneComponentTransformGizmoTarget final : public ITransformGizmoTarget
{
public:
    explicit FSceneComponentTransformGizmoTarget(USceneComponent* InComponent)
        : Component(InComponent)
    {
    }

    bool IsValid() const override { return Component != nullptr; }

    FTransform GetWorldTransform() const override
    {
        if (!Component)
        {
            return FTransform();
        }

        return FTransform(Component->GetWorldLocation(), Component->GetWorldQuat(), Component->GetWorldScale());
    }

    void SetWorldTransform(const FTransform& NewWorldTransform) override
    {
        SetWorldMatrix(NewWorldTransform.ToMatrix());
    }

    FMatrix GetWorldMatrix() const override
    {
        return Component ? Component->GetWorldMatrix() : FMatrix::Identity;
    }

    void SetWorldMatrix(const FMatrix& NewWorldMatrix) override
    {
        if (!Component)
        {
            return;
        }

        FMatrix NewRelativeMatrix = NewWorldMatrix;
        if (USceneComponent* Parent = Component->GetParent())
        {
            // Row-major convention: World = Local * ParentWorld.
            // Therefore Local = World * inverse(ParentWorld).
            NewRelativeMatrix = NewWorldMatrix * Parent->GetWorldMatrix().GetInverse();
        }

        const FTransform NewRelativeTransform = FTransform::FromMatrix(NewRelativeMatrix);
        Component->SetRelativeLocation(NewRelativeTransform.GetLocation());
        Component->SetRelativeRotation(NewRelativeTransform.Rotation);
        Component->SetRelativeScale(NewRelativeTransform.Scale);
    }

    FTransform GetLocalTransform() const override
    {
        return Component ? Component->GetRelativeTransform() : FTransform();
    }

    void SetLocalTransform(const FTransform& NewLocalTransform) override
    {
        if (!Component)
        {
            return;
        }
        Component->SetRelativeLocation(NewLocalTransform.GetLocation());
        Component->SetRelativeRotation(NewLocalTransform.Rotation);
        Component->SetRelativeScale(NewLocalTransform.Scale);
    }

private:
    USceneComponent* Component = nullptr;
};
