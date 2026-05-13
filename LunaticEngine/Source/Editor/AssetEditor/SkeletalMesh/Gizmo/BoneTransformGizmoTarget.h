#pragma once

#include "Component/Gizmo/TransformGizmoTarget.h"
#include "Core/CoreTypes.h"

class USkeletalMeshComponent;

// SkeletalMeshEditor 전용 Bone Transform Gizmo Target.
// Bone은 SceneComponent가 아니므로 BoneIndex를 통해 USkeletalMeshComponent의 Pose를 수정한다.
class FBoneTransformGizmoTarget final : public ITransformGizmoTarget
{
public:
    FBoneTransformGizmoTarget(USkeletalMeshComponent* InComponent, int32 InBoneIndex);

    bool IsValid() const override;

    FTransform GetWorldTransform() const override;
    void SetWorldTransform(const FTransform& NewWorldTransform) override;
    FMatrix GetWorldMatrix() const override;
    void SetWorldMatrix(const FMatrix& NewWorldMatrix) override;

    FTransform GetLocalTransform() const override;
    void SetLocalTransform(const FTransform& NewLocalTransform) override;

    void BeginTransform() override;
    void EndTransform() override;

    int32 GetBoneIndex() const { return BoneIndex; }

private:
    USkeletalMeshComponent* Component = nullptr;
    int32 BoneIndex = -1;
    bool bTransforming = false;
};
