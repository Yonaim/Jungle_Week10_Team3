#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/Gizmo/BoneTransformGizmoTarget.h"

#include "AssetEditor/SkeletalMesh/SkeletalMeshPreviewPoseController.h"
#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"

#include <utility>

FBoneTransformGizmoTarget::FBoneTransformGizmoTarget(USkeletalMeshComponent* InComponent,
                                                     std::shared_ptr<FSkeletalMeshPreviewPoseController> InPoseController,
                                                     int32 InBoneIndex,
                                                     const bool* InOwnerContextActiveFlag)
    : Component(InComponent)
    , PoseController(std::move(InPoseController))
    , BoneIndex(InBoneIndex)
    , OwnerContextActiveFlag(InOwnerContextActiveFlag)
{
}

bool FBoneTransformGizmoTarget::IsValid() const
{
    if (!OwnerContextActiveFlag || !(*OwnerContextActiveFlag))
    {
        return false;
    }

    if (!Component || BoneIndex < 0)
    {
        return false;
    }

    if (PoseController)
    {
        return PoseController->IsBoneValid(BoneIndex);
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

    if (PoseController)
    {
        return PoseController->GetBoneComponentTransform(BoneIndex);
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

    if (PoseController)
    {
        return PoseController->GetBoneComponentMatrix(BoneIndex);
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

    const FSkeletalMesh* MeshAsset = Component->GetSkeletalMesh()->GetSkeletalMeshAsset();
    const int32 ParentIndex = MeshAsset->Bones[BoneIndex].ParentIndex;

    FMatrix NewLocalMatrix = NewWorldMatrix;
    if (ParentIndex != InvalidBoneIndex)
    {
        FMatrix ParentComponentMatrix = FMatrix::Identity;
        if (PoseController)
        {
            ParentComponentMatrix = PoseController->GetBoneComponentMatrix(ParentIndex);
        }
        else
        {
            const FSkeletonPose& Pose = Component->GetCurrentPose();
            if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(Pose.ComponentTransforms.size()))
            {
                ParentComponentMatrix = Pose.ComponentTransforms[ParentIndex];
            }
        }

        NewLocalMatrix = NewWorldMatrix * ParentComponentMatrix.GetInverse();
    }
    SetLocalTransform(FTransform::FromMatrix(NewLocalMatrix));
}

FTransform FBoneTransformGizmoTarget::GetLocalTransform() const
{
    if (!IsValid())
    {
        return FTransform();
    }

    if (PoseController)
    {
        return PoseController->GetBoneLocalTransform(BoneIndex);
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

    if (PoseController)
    {
        PoseController->SetBoneLocalTransformFromUI(BoneIndex, NewLocalTransform);
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
    if (!PoseController && bTransforming && Component)
    {
        Component->RefreshSkinningForEditor(0.0f);
    }

    bTransforming = false;
}

bool FBoneTransformGizmoTarget::BeginGizmoDrag(const FGizmoDragBeginContext& Context)
{
    if (!PoseController || !IsValid())
    {
        return false;
    }

    return PoseController->BeginBoneGizmoSession(BoneIndex, Context);
}

bool FBoneTransformGizmoTarget::ApplyGizmoDelta(const FGizmoDelta& Delta)
{
    if (!PoseController || !IsValid())
    {
        return false;
    }

    return PoseController->ApplyBoneGizmoDelta(BoneIndex, Delta);
}

void FBoneTransformGizmoTarget::EndGizmoDrag(bool bCancelled)
{
    if (!PoseController)
    {
        return;
    }

    PoseController->EndBoneGizmoSession(BoneIndex, bCancelled);
}
