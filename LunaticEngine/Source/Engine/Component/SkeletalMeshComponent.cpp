#include "SkeletalMeshComponent.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& LocalTransform)
{
}

void USkeletalMeshComponent::SetBoneLocalTransformByName(const FString& BoneName, const FTransform& LocalTransform)
{
}

void USkeletalMeshComponent::BeginPlay()
{
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
}

void USkeletalMeshComponent::EvaluatePose(float DeltaTime)
{
}

void USkeletalMeshComponent::FillComponentSpaceTransforms()
{
}

void USkeletalMeshComponent::PerformCPUSkinning()
{
}

void USkeletalMeshComponent::FinalizeBoneTransforms()
{
}
