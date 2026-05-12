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
        return FTransform(Component->GetWorldLocation(), Component->GetComponentRotation(), Component->GetWorldScale());
    }

    void SetWorldTransform(const FTransform& NewWorldTransform) override
    {
        if (!Component)
        {
            return;
        }
        Component->SetWorldLocation(NewWorldTransform.GetLocation());
        // 기즈모 회전은 매 프레임 delta quat으로 누적된다. 여기서 Rotator로
        // 라운드트립하면 짐벌락/축 뒤집힘이 다시 생기므로 quat 그대로 적용한다.
        Component->SetWorldRotation(NewWorldTransform.Rotation);
        Component->SetRelativeScale(NewWorldTransform.Scale);
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
