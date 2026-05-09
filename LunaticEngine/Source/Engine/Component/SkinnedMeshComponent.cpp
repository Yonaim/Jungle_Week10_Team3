#include "SkinnedMeshComponent.h"
#include "SkeletalMeshProxy.h"
#include <algorithm>

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
	if (BoneIndex < 0 || BoneIndex >= (int32)ComponentSpaceTransforms.size())
		return FTransform();

	FMatrix BoneWorldMatrix = ComponentSpaceTransforms[BoneIndex] * GetWorldMatrix();
	return FTransform(
		BoneWorldMatrix.GetLocation(),
		BoneWorldMatrix.ToQuat(),
		BoneWorldMatrix.GetScale()
	);
}

FTransform USkinnedMeshComponent::GetBoneWorldTransformByName(const FString& BoneName) const
{
	// Skeleton이 bone name->index 매핑을 제공하면
	return FTransform();
}

int32 USkinnedMeshComponent::GetBoneCount() const
{
	return (int32)BoneSpaceTransforms.size();
}

FPrimitiveSceneProxy* USkinnedMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshProxy(this);
}

void USkinnedMeshComponent::UpdateWorldAABB() const
{
	TArray<FNormalVertex>* Verts = const_cast<USkinnedMeshComponent*>(this)->GetCPUSkinnedVertices();
	if (!Verts || Verts->empty())
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector LocalMin = (*Verts)[0].pos;
	FVector LocalMax = (*Verts)[0].pos;
	for (const FNormalVertex& V : *Verts)
	{
		LocalMin.X = std::min(LocalMin.X, V.pos.X);
		LocalMin.Y = std::min(LocalMin.Y, V.pos.Y);
		LocalMin.Z = std::min(LocalMin.Z, V.pos.Z);
		LocalMax.X = std::max(LocalMax.X, V.pos.X);
		LocalMax.Y = std::max(LocalMax.Y, V.pos.Y);
		LocalMax.Z = std::max(LocalMax.Z, V.pos.Z);
	}

	FVector LocalCenter = (LocalMin + LocalMax) * 0.5f;
	FVector LocalExtent = (LocalMax - LocalMin) * 0.5f;

	const FMatrix& W = GetWorldMatrix();
	FVector WorldCenter = W.TransformPositionWithW(LocalCenter);

	float Ex = std::abs(W.M[0][0]) * LocalExtent.X + std::abs(W.M[1][0]) * LocalExtent.Y + std::abs(W.M[2][0]) * LocalExtent.Z;
	float Ey = std::abs(W.M[0][1]) * LocalExtent.X + std::abs(W.M[1][1]) * LocalExtent.Y + std::abs(W.M[2][1]) * LocalExtent.Z;
	float Ez = std::abs(W.M[0][2]) * LocalExtent.X + std::abs(W.M[1][2]) * LocalExtent.Y + std::abs(W.M[2][2]) * LocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

void USkinnedMeshComponent::InitBoneTransform()
{

}
