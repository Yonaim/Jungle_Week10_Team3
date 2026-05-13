#pragma once
#include "SkinnedMeshComponent.h"

// 시연용
#include "Math/Rotator.h"

// USkeletalMeshComponent:
// 실제 SkeletalMesh 인스턴스를 월드에 배치하고 렌더링하는 구체 컴포넌트.
// 본 포즈를 평가하고 CPU Skinning 결과를 렌더러가 사용할 버퍼로 유지한다.
class USkeletalMeshComponent : public USkinnedMeshComponent
{
public:
	DECLARE_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

	USkeletalMeshComponent() = default;
	~USkeletalMeshComponent() override = default;

	bool SetBoneLocalTransform(int32 BoneIndex, const FTransform& LocalTransform);
	bool ApplyLocalPoseTransforms(const TArray<FTransform>& LocalTransforms);
	void ResetToBindPose();
	void SetBoneLocalTransformByName(const FString& BoneName, const FTransform& LocalTransform);

	TArray<FNormalVertex>* GetCPUSkinnedVertices() override { return &SkinBuffer; }

	void SetSkeletalMesh(USkeletalMesh* Mesh);
	void RefreshSkinningForEditor(float DeltaTime);
	void RefreshSkinningNow();

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;

	// 시연용
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;
	void Serialize(FArchive& Ar) override;

protected:
	void InitializeSkeleton();
	virtual void EvaluatePose(float DeltaTime);
	void UpdatePoseLocal(float DeltaTime);
	void RebuildComponentSpace();
	// Reference Pose 정점 + BoneIndex/Weight + 현재 본 행렬을 사용해 현재 포즈 정점을 계산한다.
	void PerformCPUSkinning(const FSkeletonPose& Pose);
	void FinalizeRenderState();

	// 컴포넌트 인스턴스가 소유하는 런타임 스키닝 결과.
	// 애셋 원본 정점(FSkeletalMesh::Vertices)은 보존하고 여기만 갱신한다.
	TArray<FNormalVertex> SkinBuffer;


	// 시연용
	// ---- PIE 본 회전 테스트 ----
	// 에디터 디테일 패널에서 설정하고 PIE(또는 에디터 프리뷰)에서 실시간 확인 가능.
	bool        bEnableBoneRotationTest = false;
	FString     TestBoneName;                         // 회전할 본 이름 (빈 문자열이면 비활성)
	FRotator    TestBoneRotationSpeed = FRotator(0.f, 90.f, 0.f); // 도/초 (Pitch/Yaw/Roll)

	// 런타임 누적값 — 직렬화하지 않음
	FRotator    TestBoneAccumulatedRotation;
	FString     LastTestBoneName;                     // 본 이름 변경 감지용
};
