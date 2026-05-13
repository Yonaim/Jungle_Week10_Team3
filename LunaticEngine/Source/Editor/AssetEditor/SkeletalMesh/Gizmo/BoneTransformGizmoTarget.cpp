#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/Gizmo/BoneTransformGizmoTarget.h"

#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"

FBoneTransformGizmoTarget::FBoneTransformGizmoTarget(USkeletalMeshComponent* InComponent, int32 InBoneIndex)
    : Component(InComponent)
    , BoneIndex(InBoneIndex)
{
}

bool FBoneTransformGizmoTarget::IsValid() const
{
    if (!Component || BoneIndex < 0)
    {
        return false;
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    return BoneIndex < static_cast<int32>(Pose.LocalTransforms.size()) &&
           BoneIndex < static_cast<int32>(Pose.ComponentTransforms.size());
}

FTransform FBoneTransformGizmoTarget::GetWorldTransform() const
{
    if (!IsValid())
    {
        return FTransform();
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    return FTransform::FromMatrix(Pose.ComponentTransforms[BoneIndex]);
}

void FBoneTransformGizmoTarget::SetWorldTransform(const FTransform& NewWorldTransform)
{
    SetWorldMatrix(NewWorldTransform.ToMatrix());
}

FMatrix FBoneTransformGizmoTarget::GetWorldMatrix() const
{
    if (!IsValid())
    {
        return FMatrix::Identity;
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    return Pose.ComponentTransforms[BoneIndex];
}

void FBoneTransformGizmoTarget::SetWorldMatrix(const FMatrix& NewWorldMatrix)
{
    if (!IsValid() || !Component || !Component->GetSkeletalMesh() || !Component->GetSkeletalMesh()->GetSkeletalMeshAsset())
    {
        return;
    }

    // Bone gizmo의 WorldMatrix는 현재 preview component-space transform이다.
    // parent가 있는 bone은 component-space 행렬을 parent component-space의 inverse로 되돌린 뒤
    // local transform으로 저장해야 부모 기준으로 한 번 더 누적되는 문제를 피할 수 있다.
    const FSkeletalMesh* MeshAsset = Component->GetSkeletalMesh()->GetSkeletalMeshAsset();
    const FSkeletonPose& Pose = Component->GetCurrentPose();
    const int32 ParentIndex = MeshAsset->Bones[BoneIndex].ParentIndex;

    FMatrix NewLocalMatrix = NewWorldMatrix;
    if (ParentIndex != InvalidBoneIndex &&
        ParentIndex >= 0 &&
        ParentIndex < static_cast<int32>(Pose.ComponentTransforms.size()))
    {
        const FMatrix ParentComponentInverse = Pose.ComponentTransforms[ParentIndex].GetInverse();
        NewLocalMatrix = NewWorldMatrix * ParentComponentInverse;
    }

    SetLocalTransform(FTransform::FromMatrix(NewLocalMatrix));
}

FTransform FBoneTransformGizmoTarget::GetLocalTransform() const
{
    if (!IsValid())
    {
        return FTransform();
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    return Pose.LocalTransforms[BoneIndex];
}

void FBoneTransformGizmoTarget::SetLocalTransform(const FTransform& NewLocalTransform)
{
    if (!IsValid())
    {
        return;
    }

    Component->SetBoneLocalTransform(BoneIndex, NewLocalTransform);
    Component->RefreshSkinningNow();
}

void FBoneTransformGizmoTarget::BeginTransform()
{
    bTransforming = IsValid();
}

void FBoneTransformGizmoTarget::EndTransform()
{
    if (!bTransforming || !Component)
    {
        bTransforming = false;
        return;
    }

    Component->RefreshSkinningForEditor(0.0f);
    bTransforming = false;
}
