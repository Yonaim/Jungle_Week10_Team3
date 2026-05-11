#include "AssetEditor/SkeletalMesh/Gizmo/BoneTransformGizmoTarget.h"

#include "Component/SkeletalMeshComponent.h"

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
    // 현재 Runtime 쪽 공개 API가 local pose 수정 중심이므로,
    // 실제 저장은 SetLocalTransform 경로로 통일한다.
    // 향후 ParentWorld 역산 API가 정리되면 여기서 World -> Local 변환을 수행하면 된다.
    SetLocalTransform(NewWorldTransform);
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
