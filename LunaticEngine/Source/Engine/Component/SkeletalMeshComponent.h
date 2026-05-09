#pragma once
#include "SkinnedMeshComponent.h"

class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	void SetBoneLocalTransform(int32 BoneIndex, const FTransform& LocalTransform);
	void SetBoneLocalTransformByName(const FString& BoneName, const FTransform& LocalTransform);

	TArray<FNormalVertex>* GetCPUSkinnedVertices() override { return &SkinBuffer; }

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

protected:
	virtual void EvaluatePose(float DeltaTime);
	void FillComponentSpaceTransforms();
	void PerformCPUSkinning();
	void FinalizeBoneTransforms();

	TArray<FNormalVertex> SkinBuffer;
};
