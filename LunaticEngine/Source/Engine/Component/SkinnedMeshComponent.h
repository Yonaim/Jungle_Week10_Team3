#pragma once
#include "Component/MeshComponent.h"

class USkeletalMesh;
struct FSkeleton;
struct FSkeletalMeshVertex;
class FPrimitiveSceneProxy;

class USkinnedMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

	USkinnedMeshComponent() = default;
	~USkinnedMeshComponent() override = default;

	void SetSkeletalMesh(USkeletalMesh* Mesh);
	USkeletalMesh* GetSkeletalMesh() const;

	const TArray<FTransform>& GetBoneSpaceTransforms() const { return BoneSpaceTransforms; }
	const TArray<FMatrix>& GetComponentSpaceTransforms() const { return ComponentSpaceTransforms; }

	FTransform GetBoneWorldTransform(int32 BoneIndex) const;
	FTransform GetBoneWorldTransformByName(const FString& BoneName) const;

	int32 GetBoneCount() const;

	virtual TArray<FSkeletalMeshVertex>* GetCPUSkinnedVertices() { return nullptr; }

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void UpdateWorldAABB() const override;

protected:
	USkeletalMesh* SkeletalMesh = nullptr;

	TArray<FTransform> BoneSpaceTransforms;
	TArray<FMatrix> ComponentSpaceTransforms;

	void InitBoneTransform();
};
