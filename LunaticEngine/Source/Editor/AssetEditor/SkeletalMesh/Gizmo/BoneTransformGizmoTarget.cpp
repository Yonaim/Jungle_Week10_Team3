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

    const TArray<FBoneInfo>* GetBones(const USkeletalMeshComponent* Component)
    {
        if (!Component || !Component->GetSkeletalMesh() || !Component->GetSkeletalMesh()->GetSkeletalMeshAsset())
        {
            return nullptr;
        }

        return &Component->GetSkeletalMesh()->GetSkeletalMeshAsset()->Bones;
    }

    FMatrix ConvertWorldToBoneLocalMatrix(
        const USkeletalMeshComponent* Component,
        int32 BoneIndex,
        const TArray<FBoneInfo>& Bones,
        const FSkeletonPose& Pose,
        const FMatrix& WorldMatrix)
    {
        const FMatrix ComponentMatrix = WorldMatrix * Component->GetWorldMatrix().GetInverse();
        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex == InvalidBoneIndex ||
            ParentIndex < 0 ||
            ParentIndex >= static_cast<int32>(Pose.ComponentTransforms.size()))
        {
            return ComponentMatrix;
        }

        return ComponentMatrix * Pose.ComponentTransforms[ParentIndex].GetInverse();
    }

    FQuat ConvertWorldToBoneLocalRotation(
        const USkeletalMeshComponent* Component,
        int32 BoneIndex,
        const TArray<FBoneInfo>& Bones,
        const FSkeletonPose& Pose,
        const FQuat& WorldRotation)
    {
        const FQuat ComponentWorldRotation = MakeTransformFromMatrix(Component->GetWorldMatrix()).Rotation.GetNormalized();
        FQuat ComponentRotation = WorldRotation * ComponentWorldRotation.Inverse();

        const int32 ParentIndex = Bones[BoneIndex].ParentIndex;
        if (ParentIndex != InvalidBoneIndex &&
            ParentIndex >= 0 &&
            ParentIndex < static_cast<int32>(Pose.ComponentTransforms.size()))
        {
            const FQuat ParentComponentRotation = MakeTransformFromMatrix(Pose.ComponentTransforms[ParentIndex]).Rotation.GetNormalized();
            ComponentRotation = ComponentRotation * ParentComponentRotation.Inverse();
        }

        return ComponentRotation.GetNormalized();
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
    const FMatrix BoneWorldMatrix = Pose.ComponentTransforms[BoneIndex] * Component->GetWorldMatrix();
    return MakeTransformFromMatrix(BoneWorldMatrix);
}

void FBoneTransformGizmoTarget::SetWorldTransform(const FTransform& NewWorldTransform)
{
    // The editable pose is stored as bone-local transforms, so convert the gizmo world transform
    // back through component space and the parent bone component transform before writing.
    const TArray<FBoneInfo>* Bones = GetBones(Component);
    if (!IsValid() || !Bones || BoneIndex >= static_cast<int32>(Bones->size()))
    {
        return;
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    const FMatrix DesiredLocalMatrix = ConvertWorldToBoneLocalMatrix(
        Component,
        BoneIndex,
        *Bones,
        Pose,
        NewWorldTransform.ToMatrix());

    SetLocalTransform(MakeTransformFromMatrix(DesiredLocalMatrix));
}

void FBoneTransformGizmoTarget::SetWorldLocation(const FVector& NewWorldLocation)
{
    const TArray<FBoneInfo>* Bones = GetBones(Component);
    if (!IsValid() || !Bones || BoneIndex >= static_cast<int32>(Bones->size()))
    {
        return;
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    FTransform Local = Pose.LocalTransforms[BoneIndex];

    const FVector DesiredComponentLocation = Component->GetWorldMatrix().GetInverse().TransformPositionWithW(NewWorldLocation);
    FVector DesiredLocalLocation = DesiredComponentLocation;
    const int32 ParentIndex = (*Bones)[BoneIndex].ParentIndex;
    if (ParentIndex != InvalidBoneIndex &&
        ParentIndex >= 0 &&
        ParentIndex < static_cast<int32>(Pose.ComponentTransforms.size()))
    {
        DesiredLocalLocation = Pose.ComponentTransforms[ParentIndex].GetInverse().TransformPositionWithW(DesiredComponentLocation);
    }

    Local.SetLocation(DesiredLocalLocation);
    SetLocalTransform(Local);
}

void FBoneTransformGizmoTarget::SetWorldRotation(const FQuat& NewWorldRotation)
{
    const TArray<FBoneInfo>* Bones = GetBones(Component);
    if (!IsValid() || !Bones || BoneIndex >= static_cast<int32>(Bones->size()))
    {
        return;
    }

    const FSkeletonPose& Pose = Component->GetCurrentPose();
    FTransform Local = Pose.LocalTransforms[BoneIndex];
    Local.SetRotation(ConvertWorldToBoneLocalRotation(Component, BoneIndex, *Bones, Pose, NewWorldRotation.GetNormalized()));
    SetLocalTransform(Local);
}

void FBoneTransformGizmoTarget::SetWorldScale(const FVector& NewWorldScale)
{
    if (!IsValid())
    {
        return;
    }

    FTransform Local = GetLocalTransform();
    Local.Scale = NewWorldScale;
    SetLocalTransform(Local);
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

    if (Component->SetBoneLocalTransform(BoneIndex, NewLocalTransform))
    {
        Component->RefreshSkinningNow();
    }
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

    Component->RefreshSkinningNow();
    bTransforming = false;
}
