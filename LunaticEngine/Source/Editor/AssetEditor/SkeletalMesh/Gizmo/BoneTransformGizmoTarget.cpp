#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/Gizmo/BoneTransformGizmoTarget.h"

#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"

namespace
{
    FTransform MakeTransformFromMatrix(const FMatrix& Matrix)
    {
        FVector AxisX(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
        FVector AxisY(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
        FVector AxisZ(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

        FVector Scale(AxisX.Length(), AxisY.Length(), AxisZ.Length());
        if (Scale.X > 1e-6f)
        {
            AxisX /= Scale.X;
        }
        if (Scale.Y > 1e-6f)
        {
            AxisY /= Scale.Y;
        }
        if (Scale.Z > 1e-6f)
        {
            AxisZ /= Scale.Z;
        }

        FMatrix RotationMatrix = FMatrix::Identity;
        RotationMatrix.M[0][0] = AxisX.X;
        RotationMatrix.M[0][1] = AxisX.Y;
        RotationMatrix.M[0][2] = AxisX.Z;
        RotationMatrix.M[1][0] = AxisY.X;
        RotationMatrix.M[1][1] = AxisY.Y;
        RotationMatrix.M[1][2] = AxisY.Z;
        RotationMatrix.M[2][0] = AxisZ.X;
        RotationMatrix.M[2][1] = AxisZ.Y;
        RotationMatrix.M[2][2] = AxisZ.Z;

        return FTransform(Matrix.GetLocation(), FQuat::FromMatrix(RotationMatrix), Scale);
    }
}

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
    return MakeTransformFromMatrix(Pose.ComponentTransforms[BoneIndex]);
}

void FBoneTransformGizmoTarget::SetWorldTransform(const FTransform& NewWorldTransform)
{
    if (!IsValid() || !Component || !Component->GetSkeletalMesh() || !Component->GetSkeletalMesh()->GetSkeletalMeshAsset())
    {
        return;
    }

    // Bone gizmo의 WorldTransform은 현재 preview component space transform이다.
    // 이를 그대로 LocalTransforms[BoneIndex]에 넣으면 parent가 있는 본에서 회전/이동이
    // 부모 기준으로 한 번 더 누적되어 기즈모 회전이 크게 틀어진다.
    const FSkeletalMesh* MeshAsset = Component->GetSkeletalMesh()->GetSkeletalMeshAsset();
    const FSkeletonPose& Pose = Component->GetCurrentPose();
    const int32 ParentIndex = MeshAsset->Bones[BoneIndex].ParentIndex;

    FMatrix NewComponentMatrix = NewWorldTransform.ToMatrix();
    FTransform NewLocalTransform = NewWorldTransform;
    if (ParentIndex != InvalidBoneIndex &&
        ParentIndex >= 0 &&
        ParentIndex < static_cast<int32>(Pose.ComponentTransforms.size()))
    {
        const FMatrix ParentComponentInverse = Pose.ComponentTransforms[ParentIndex].GetInverse();
        NewLocalTransform = MakeTransformFromMatrix(NewComponentMatrix * ParentComponentInverse);
    }

    SetLocalTransform(NewLocalTransform);
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
