#pragma once
#include "Component/MeshComponent.h"
#include "Mesh/SkeletalMesh.h"

class FPrimitiveSceneProxy;

class USkinnedMeshComponent : public UMeshComponent
{
public:
	DECLARE_CLASS(USkinnedMeshComponent, UMeshComponent)

	USkinnedMeshComponent() = default;
	~USkinnedMeshComponent() override = default;

	virtual void SetSkeletalMesh(USkeletalMesh* Mesh);
	USkeletalMesh* GetSkeletalMesh() const;

	const TArray<FTransform>& GetBoneSpaceTransforms() const { return BoneSpaceTransforms; }
	const TArray<FMatrix>& GetComponentSpaceTransforms() const { return ComponentSpaceTransforms; }

	FTransform GetBoneWorldTransform(int32 BoneIndex) const;
	FTransform GetBoneWorldTransformByName(const FString& BoneName) const;

	int32 GetBoneCount() const;

	virtual TArray<FNormalVertex>* GetCPUSkinnedVertices() { return nullptr; }

	FPrimitiveSceneProxy* CreateSceneProxy() override;
	void UpdateWorldAABB() const override;

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;
	void PostDuplicate() override;

protected:
	USkeletalMesh* SkeletalMesh = nullptr;
	FString SkeletalMeshPath = "None";

	TArray<FTransform> BoneSpaceTransforms;
	TArray<FMatrix> ComponentSpaceTransforms;

	void InitBoneTransform();
};
