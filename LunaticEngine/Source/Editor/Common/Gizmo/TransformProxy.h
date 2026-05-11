#pragma once

#include "Math/Transform.h"

// GizmoManager가 실제 대상 타입을 모르도록 막는 공용 Transform Proxy.
// Bone / Socket / Actor / Component 등은 이 인터페이스만 맞추면 같은 GizmoManager로 조작할 수 있다.
class ITransformProxy
{
public:
    virtual ~ITransformProxy() = default;

    virtual bool IsValid() const = 0;

    virtual FTransform GetWorldTransform() const = 0;
    virtual void SetWorldTransform(const FTransform& NewWorldTransform) = 0;

    virtual FTransform GetLocalTransform() const = 0;
    virtual void SetLocalTransform(const FTransform& NewLocalTransform) = 0;

    virtual void BeginTransform() {}
    virtual void EndTransform() {}
};
