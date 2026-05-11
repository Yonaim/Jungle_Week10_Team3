#pragma once

#include "Math/Transform.h"

// Transform을 가진 조작 대상을 추상화하는 엔진 공용 인터페이스.
// 구현체는 Editor/Runtime 어느 쪽에 있어도 되지만, 인터페이스 자체는
// UGizmoComponent / FGizmoManager가 공통으로 참조할 수 있도록 Engine에 둔다.
class ITransformGizmoTarget
{
public:
    virtual ~ITransformGizmoTarget() = default;

    virtual bool IsValid() const = 0;

    virtual FTransform GetWorldTransform() const = 0;
    virtual void SetWorldTransform(const FTransform& NewWorldTransform) = 0;

    virtual FTransform GetLocalTransform() const = 0;
    virtual void SetLocalTransform(const FTransform& NewLocalTransform) = 0;

    virtual void BeginTransform() {}
    virtual void EndTransform() {}
};
