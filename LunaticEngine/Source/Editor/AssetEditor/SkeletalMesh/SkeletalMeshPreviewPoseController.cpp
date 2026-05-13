#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshPreviewPoseController.h"

#include "Component/SkeletalMeshComponent.h"
#include "Engine/Mesh/SkeletalMesh.h"

#include <algorithm>
#include <cmath>

namespace
{
const FSkeletalMesh* GetSkeletalMeshAsset(const USkeletalMeshComponent* Component)
{
    if (!Component || !Component->GetSkeletalMesh())
    {
        return nullptr;
    }

    return Component->GetSkeletalMesh()->GetSkeletalMeshAsset();
}
}

FSkeletalMeshPreviewPoseController::FSkeletalMeshPreviewPoseController(USkeletalMeshComponent* InPreviewComponent)
{
    BindPreviewComponent(InPreviewComponent);
}

void FSkeletalMeshPreviewPoseController::BindPreviewComponent(USkeletalMeshComponent* InPreviewComponent)
{
    if (PreviewComponent == InPreviewComponent)
    {
        return;
    }

    PreviewComponent = InPreviewComponent;
    ActiveSession = FBoneGizmoEditSession{};
    InitializeFromComponentPose();
}

void FSkeletalMeshPreviewPoseController::InitializeFromComponentPose()
{
    ActiveSession = FBoneGizmoEditSession{};

    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!PreviewComponent || !MeshAsset)
    {
        WorkingPose.Reset();
        bPoseInitialized = false;
        return;
    }

    WorkingPose = PreviewComponent->GetCurrentPose();
    const int32 BoneCount = static_cast<int32>(MeshAsset->Bones.size());
    bPoseInitialized =
        static_cast<int32>(WorkingPose.LocalTransforms.size()) == BoneCount &&
        static_cast<int32>(WorkingPose.ComponentTransforms.size()) == BoneCount;

    if (!bPoseInitialized)
    {
        WorkingPose.Reset();
    }
}

bool FSkeletalMeshPreviewPoseController::IsBoneValid(int32 BoneIndex) const
{
    EnsurePoseInitialized();
    return BoneIndex >= 0 &&
           BoneIndex < static_cast<int32>(WorkingPose.LocalTransforms.size()) &&
           BoneIndex < static_cast<int32>(WorkingPose.ComponentTransforms.size());
}

FTransform FSkeletalMeshPreviewPoseController::GetBoneLocalTransform(int32 BoneIndex) const
{
    if (!IsBoneValid(BoneIndex))
    {
        return FTransform();
    }

    return WorkingPose.LocalTransforms[BoneIndex];
}

FTransform FSkeletalMeshPreviewPoseController::GetBoneComponentTransform(int32 BoneIndex) const
{
    if (!IsBoneValid(BoneIndex))
    {
        return FTransform();
    }

    return FTransform::FromMatrix(WorkingPose.ComponentTransforms[BoneIndex]);
}

FMatrix FSkeletalMeshPreviewPoseController::GetBoneComponentMatrix(int32 BoneIndex) const
{
    if (!IsBoneValid(BoneIndex))
    {
        return FMatrix::Identity;
    }

    return WorkingPose.ComponentTransforms[BoneIndex];
}

bool FSkeletalMeshPreviewPoseController::BeginBoneGizmoSession(int32 BoneIndex, const FGizmoDragBeginContext& Context)
{
    if (!IsBoneValid(BoneIndex))
    {
        return false;
    }

    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!MeshAsset || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
    {
        return false;
    }

    ActiveSession = FBoneGizmoEditSession{};
    ActiveSession.bActive = true;
    ActiveSession.BoneIndex = BoneIndex;
    ActiveSession.DragContext = Context;
    ActiveSession.StartLocalTransform = WorkingPose.LocalTransforms[BoneIndex];

    const int32 ParentIndex = MeshAsset->Bones[BoneIndex].ParentIndex;
    if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(WorkingPose.ComponentTransforms.size()))
    {
        ActiveSession.bHasParent = true;
        ActiveSession.StartParentComponentMatrix = WorkingPose.ComponentTransforms[ParentIndex];
    }

    return true;
}

bool FSkeletalMeshPreviewPoseController::ApplyBoneGizmoDelta(int32 BoneIndex, const FGizmoDelta& Delta)
{
    if (!ActiveSession.bActive || ActiveSession.BoneIndex != BoneIndex)
    {
        return false;
    }

    switch (Delta.Mode)
    {
    case EGizmoMode::Translate:
        return ApplyTranslationDelta(BoneIndex, Delta);
    case EGizmoMode::Rotate:
        return ApplyRotationDelta(BoneIndex, Delta);
    case EGizmoMode::Scale:
        return ApplyScaleDelta(BoneIndex, Delta);
    default:
        return false;
    }
}

void FSkeletalMeshPreviewPoseController::EndBoneGizmoSession(int32 BoneIndex, bool bCancelled)
{
    if (!ActiveSession.bActive || ActiveSession.BoneIndex != BoneIndex)
    {
        ActiveSession = FBoneGizmoEditSession{};
        return;
    }

    if (bCancelled)
    {
        ApplyLocalTransform(BoneIndex, ActiveSession.StartLocalTransform);
    }

    ActiveSession = FBoneGizmoEditSession{};
}

bool FSkeletalMeshPreviewPoseController::SetBoneLocalTransformFromUI(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    if (!IsBoneValid(BoneIndex))
    {
        return false;
    }

    return ApplyLocalTransform(BoneIndex, NewLocalTransform);
}

bool FSkeletalMeshPreviewPoseController::ResetBoneLocalTransform(int32 BoneIndex)
{
    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!MeshAsset || BoneIndex < 0 || BoneIndex >= static_cast<int32>(MeshAsset->Bones.size()))
    {
        return false;
    }

    return ApplyLocalTransform(BoneIndex, MeshAsset->Bones[BoneIndex].LocalBindTransform);
}

void FSkeletalMeshPreviewPoseController::ResetAllBoneTransforms()
{
    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!MeshAsset)
    {
        WorkingPose.Reset();
        bPoseInitialized = false;
        ActiveSession = FBoneGizmoEditSession{};
        return;
    }

    WorkingPose.InitializeFromBindPose(*MeshAsset);
    bPoseInitialized = true;
    ActiveSession = FBoneGizmoEditSession{};
    PushWorkingPoseToPreviewComponent();
}

void FSkeletalMeshPreviewPoseController::EnsurePoseInitialized() const
{
    if (!bPoseInitialized)
    {
        const_cast<FSkeletalMeshPreviewPoseController*>(this)->InitializeFromComponentPose();
    }
}

FQuat FSkeletalMeshPreviewPoseController::ExtractRotationFromMatrix(const FMatrix& Matrix) const
{
    FVector AxisX(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
    FVector AxisY(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);

    const float ScaleX = AxisX.Length();
    const float ScaleY = AxisY.Length();
    AxisX = (ScaleX > 1.0e-6f) ? (AxisX / ScaleX) : FVector::ForwardVector;
    AxisY = (ScaleY > 1.0e-6f) ? (AxisY / ScaleY) : FVector::RightVector;

    if (Matrix.GetBasisDeterminant3x3() < 0.0f)
    {
        AxisX *= -1.0f;
    }

    AxisX = AxisX.Normalized();
    AxisY = (AxisY - AxisX * AxisY.Dot(AxisX)).Normalized();
    if (AxisY.IsNearlyZero())
    {
        AxisY = (std::abs(AxisX.Dot(FVector::UpVector)) < 0.95f)
            ? (FVector::UpVector - AxisX * FVector::UpVector.Dot(AxisX)).Normalized()
            : (FVector::RightVector - AxisX * FVector::RightVector.Dot(AxisX)).Normalized();
    }

    FVector AxisZ = AxisX.Cross(AxisY).Normalized();
    if (AxisZ.IsNearlyZero())
    {
        AxisZ = FVector::UpVector;
    }
    AxisY = AxisZ.Cross(AxisX).Normalized();

    FMatrix RotationMatrix = FMatrix::Identity;
    RotationMatrix.M[0][0] = AxisX.X; RotationMatrix.M[0][1] = AxisX.Y; RotationMatrix.M[0][2] = AxisX.Z;
    RotationMatrix.M[1][0] = AxisY.X; RotationMatrix.M[1][1] = AxisY.Y; RotationMatrix.M[1][2] = AxisY.Z;
    RotationMatrix.M[2][0] = AxisZ.X; RotationMatrix.M[2][1] = AxisZ.Y; RotationMatrix.M[2][2] = AxisZ.Z;
    return RotationMatrix.ToQuat().GetNormalized();
}

bool FSkeletalMeshPreviewPoseController::DecomposeMatrixWithReference(
    const FMatrix& Matrix,
    const FTransform& ReferenceLocalTransform,
    FTransform& OutTransform) const
{
    const FVector RawAxisX(Matrix.M[0][0], Matrix.M[0][1], Matrix.M[0][2]);
    const FVector RawAxisY(Matrix.M[1][0], Matrix.M[1][1], Matrix.M[1][2]);
    const FVector RawAxisZ(Matrix.M[2][0], Matrix.M[2][1], Matrix.M[2][2]);

    const float MagnitudeX = RawAxisX.Length();
    const float MagnitudeY = RawAxisY.Length();
    const float MagnitudeZ = RawAxisZ.Length();

    const FVector ReferenceAxisX = ReferenceLocalTransform.Rotation.GetForwardVector().Normalized();
    const FVector ReferenceAxisY = ReferenceLocalTransform.Rotation.GetRightVector().Normalized();
    const FVector ReferenceAxisZ = ReferenceLocalTransform.Rotation.GetUpVector().Normalized();

    const float SignX = (MagnitudeX > 1.0e-6f && RawAxisX.Dot(ReferenceAxisX) < 0.0f) ? -1.0f : 1.0f;
    const float SignY = (MagnitudeY > 1.0e-6f && RawAxisY.Dot(ReferenceAxisY) < 0.0f) ? -1.0f : 1.0f;
    const float SignZ = (MagnitudeZ > 1.0e-6f && RawAxisZ.Dot(ReferenceAxisZ) < 0.0f) ? -1.0f : 1.0f;

    FVector Scale(
        (MagnitudeX > 1.0e-6f) ? (MagnitudeX * SignX) : ReferenceLocalTransform.Scale.X,
        (MagnitudeY > 1.0e-6f) ? (MagnitudeY * SignY) : ReferenceLocalTransform.Scale.Y,
        (MagnitudeZ > 1.0e-6f) ? (MagnitudeZ * SignZ) : ReferenceLocalTransform.Scale.Z);

    FVector AxisX = (std::abs(Scale.X) > 1.0e-6f) ? (RawAxisX / Scale.X) : ReferenceAxisX;
    FVector AxisY = (std::abs(Scale.Y) > 1.0e-6f) ? (RawAxisY / Scale.Y) : ReferenceAxisY;
    FVector AxisZ = (std::abs(Scale.Z) > 1.0e-6f) ? (RawAxisZ / Scale.Z) : ReferenceAxisZ;

    AxisX = AxisX.Normalized();
    AxisY = (AxisY - AxisX * AxisY.Dot(AxisX)).Normalized();
    if (AxisY.IsNearlyZero())
    {
        AxisY = ReferenceAxisY;
    }
    AxisZ = AxisX.Cross(AxisY).Normalized();
    if (AxisZ.IsNearlyZero())
    {
        AxisZ = ReferenceAxisZ;
    }
    AxisY = AxisZ.Cross(AxisX).Normalized();
    if (AxisY.IsNearlyZero())
    {
        AxisY = ReferenceAxisY;
    }

    FMatrix RotationMatrix = FMatrix::Identity;
    RotationMatrix.M[0][0] = AxisX.X; RotationMatrix.M[0][1] = AxisX.Y; RotationMatrix.M[0][2] = AxisX.Z;
    RotationMatrix.M[1][0] = AxisY.X; RotationMatrix.M[1][1] = AxisY.Y; RotationMatrix.M[1][2] = AxisY.Z;
    RotationMatrix.M[2][0] = AxisZ.X; RotationMatrix.M[2][1] = AxisZ.Y; RotationMatrix.M[2][2] = AxisZ.Z;

    OutTransform.Location = Matrix.GetLocation();
    OutTransform.Rotation = RotationMatrix.ToQuat().GetNormalized();
    OutTransform.Scale = Scale;
    return true;
}

FVector FSkeletalMeshPreviewPoseController::ConvertComponentDeltaToParentDelta(
    const FBoneGizmoEditSession& Session,
    const FVector& DeltaComponent) const
{
    if (!Session.bHasParent)
    {
        return DeltaComponent;
    }

    const FMatrix ParentInverse = Session.StartParentComponentMatrix.GetInverse();
    return ParentInverse.TransformVector(DeltaComponent);
}

float FSkeletalMeshPreviewPoseController::ApplySignedScaleDelta(float StartScale, float DeltaMagnitude) const
{
    const float Sign = (StartScale < 0.0f) ? -1.0f : 1.0f;
    const float NewMagnitude = (std::max)(0.001f, std::abs(StartScale) + DeltaMagnitude);
    return Sign * NewMagnitude;
}

bool FSkeletalMeshPreviewPoseController::ApplyTranslationDelta(int32 BoneIndex, const FGizmoDelta& Delta)
{
    const FVector DeltaParent = ConvertComponentDeltaToParentDelta(ActiveSession, Delta.LinearDeltaComponent);

    FTransform NewLocal = ActiveSession.StartLocalTransform;
    NewLocal.Location = ActiveSession.StartLocalTransform.Location + DeltaParent;
    return ApplyLocalTransform(BoneIndex, NewLocal);
}

bool FSkeletalMeshPreviewPoseController::ApplyRotationDelta(int32 BoneIndex, const FGizmoDelta& Delta)
{
    if (std::abs(Delta.AngularDeltaRadians) <= FMath::Epsilon || Delta.AxisVectorComponent.IsNearlyZero())
    {
        return true;
    }

    const FVector AxisComponent = Delta.AxisVectorComponent.Normalized();
    const FQuat DeltaWorldQuat = FQuat::FromAxisAngle(AxisComponent, Delta.AngularDeltaRadians);
    const FQuat ParentWorldQuat = ActiveSession.bHasParent
        ? ExtractRotationFromMatrix(ActiveSession.StartParentComponentMatrix)
        : FQuat::Identity;
    const FQuat StartWorldQuat = ActiveSession.bHasParent
        ? (ActiveSession.StartLocalTransform.Rotation * ParentWorldQuat).GetNormalized()
        : ActiveSession.StartLocalTransform.Rotation.GetNormalized();
    const FQuat NewWorldQuat = (DeltaWorldQuat * StartWorldQuat).GetNormalized();
    const FQuat NewLocalQuat = ActiveSession.bHasParent
        ? (NewWorldQuat * ParentWorldQuat.Inverse()).GetNormalized()
        : NewWorldQuat;

    FTransform NewLocal = ActiveSession.StartLocalTransform;
    NewLocal.Rotation = NewLocalQuat;
    return ApplyLocalTransform(BoneIndex, NewLocal);
}

bool FSkeletalMeshPreviewPoseController::ApplyScaleDelta(int32 BoneIndex, const FGizmoDelta& Delta)
{
    const int32 Axis = Delta.ActiveAxis;
    if (Axis < 0 || Axis > 2)
    {
        return false;
    }

    if (Delta.Space == EGizmoSpace::Local)
    {
        FTransform NewLocal = ActiveSession.StartLocalTransform;
        if (Axis == 0) NewLocal.Scale.X = ApplySignedScaleDelta(ActiveSession.StartLocalTransform.Scale.X, Delta.ScaleDelta.X);
        if (Axis == 1) NewLocal.Scale.Y = ApplySignedScaleDelta(ActiveSession.StartLocalTransform.Scale.Y, Delta.ScaleDelta.Y);
        if (Axis == 2) NewLocal.Scale.Z = ApplySignedScaleDelta(ActiveSession.StartLocalTransform.Scale.Z, Delta.ScaleDelta.Z);
        return ApplyLocalTransform(BoneIndex, NewLocal);
    }

    FVector WorldScaleFactor(1.0f, 1.0f, 1.0f);
    if (Axis == 0) WorldScaleFactor.X = (std::max)(0.001f, 1.0f + Delta.ScaleDelta.X);
    if (Axis == 1) WorldScaleFactor.Y = (std::max)(0.001f, 1.0f + Delta.ScaleDelta.Y);
    if (Axis == 2) WorldScaleFactor.Z = (std::max)(0.001f, 1.0f + Delta.ScaleDelta.Z);

    const FVector Center = ActiveSession.DragContext.StartTargetTransform.GetLocation();
    const FMatrix ToCenter = FMatrix::MakeTranslationMatrix(Center * -1.0f);
    const FMatrix WorldScaleMatrix = FMatrix::MakeScaleMatrix(WorldScaleFactor);
    const FMatrix FromCenter = FMatrix::MakeTranslationMatrix(Center);
    const FMatrix NewWorldMatrix = ActiveSession.DragContext.StartTargetWorldMatrix * ToCenter * WorldScaleMatrix * FromCenter;
    const FMatrix NewLocalMatrix = ActiveSession.bHasParent
        ? (NewWorldMatrix * ActiveSession.StartParentComponentMatrix.GetInverse())
        : NewWorldMatrix;

    FTransform NewLocal;
    if (!DecomposeMatrixWithReference(NewLocalMatrix, ActiveSession.StartLocalTransform, NewLocal))
    {
        return false;
    }

    return ApplyLocalTransform(BoneIndex, NewLocal);
}

bool FSkeletalMeshPreviewPoseController::ApplyLocalTransform(int32 BoneIndex, const FTransform& NewLocalTransform)
{
    const FSkeletalMesh* MeshAsset = GetSkeletalMeshAsset(PreviewComponent);
    if (!MeshAsset || !IsBoneValid(BoneIndex))
    {
        return false;
    }

    if (!WorkingPose.SetLocalTransform(BoneIndex, NewLocalTransform))
    {
        return false;
    }

    WorkingPose.RebuildComponentSpace(MeshAsset->Bones);
    PushWorkingPoseToPreviewComponent();
    return true;
}

void FSkeletalMeshPreviewPoseController::PushWorkingPoseToPreviewComponent()
{
    if (!PreviewComponent || !bPoseInitialized)
    {
        return;
    }

    PreviewComponent->SetPreviewPoseForEditor(WorkingPose);
}
