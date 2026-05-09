#include "SkeletalMeshProxy.h"

FSkeletalMeshProxy::FSkeletalMeshProxy(USkinnedMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{

}

USkinnedMeshComponent* FSkeletalMeshProxy::GetSkinnedMeshComponent() const
{
	return static_cast<USkinnedMeshComponent*>(GetOwner());
}

void FSkeletalMeshProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FSkeletalMeshProxy::UpdateMesh()
{
	MeshBuffer = GetOwner()
}

void FSkeletalMeshProxy::UpdateLOD(uint32 LODLevel)
{
}

void FSkeletalMeshProxy::UpdatePerViewport(const FFrameContext& Frame)
{
}

void FSkeletalMeshProxy::RebuildSectionDraws()
{

}


