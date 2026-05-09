#include "SkinnedMeshComponent.h"

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* Mesh)
{
	SkeletalMesh = Mesh;
}

USkeletalMesh* USkinnedMeshComponent::GetSkeletalMesh() const
{
	return SkeletalMesh;
}

FTransform USkinnedMeshComponent::GetBoneWorldTransform(int32 BoneIndex) const
{


	return FTransform();
}

FTransform USkinnedMeshComponent::GetBoneWorldTransformByName(const FString& BoneName) const
{
	return FTransform();
}

int32 USkinnedMeshComponent::GetBoneCount() const
{
	return int32();
}

FPrimitiveSceneProxy* USkinnedMeshComponent::CreateSceneProxy()
{
	return nullptr;
}

void USkinnedMeshComponent::UpdateWorldAABB() const
{
}

void USkinnedMeshComponent::InitBoneTransform()
{
}
