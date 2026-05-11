#include "SkeletalMeshComponent.h"
#include "Serialization/Archive.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshCommon.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

bool USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& LocalTransform)
{
	return CurrentPose.SetLocalTransform(BoneIndex, LocalTransform);
}

void USkeletalMeshComponent::SetBoneLocalTransformByName(const FString& BoneName, const FTransform& LocalTransform)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;
	for (int32 i = 0; i < (int32)Bones.size(); ++i)
	{
		if (Bones[i].Name == BoneName)
		{
			SetBoneLocalTransform(i, LocalTransform);
			return;
		}
	}
}

// 메시 할당 시 즉시 초기화 — 에디터에서 BeginPlay 없이도 렌더링되도록
void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* Mesh)
{
	USkinnedMeshComponent::SetSkeletalMesh(Mesh);
	InitializeSkeleton();
}

void USkeletalMeshComponent::InitializeSkeleton()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
	InitBoneTransform();

	// SkinBuffer는 매 프레임 갱신되는 런타임 결과 버퍼이며,
	// 원본 애셋 정점은 참조 포즈 보존용으로 유지한다.
	SkinBuffer = MeshAsset->Vertices;

	RebuildComponentSpace();
	PerformCPUSkinning(CurrentPose);
	FinalizeRenderState();
}

void USkeletalMeshComponent::BeginPlay()
{
	InitializeSkeleton();
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	EvaluatePose(DeltaTime);
	UpdatePoseLocal(DeltaTime);
	RebuildComponentSpace();
	PerformCPUSkinning(CurrentPose);
	FinalizeRenderState();
}

void USkeletalMeshComponent::RefreshSkinningForEditor(float DeltaTime)
{
    if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
        return;

    EvaluatePose(DeltaTime);
    UpdatePoseLocal(DeltaTime);
    RebuildComponentSpace();
    PerformCPUSkinning(CurrentPose);
    FinalizeRenderState();
}

void USkeletalMeshComponent::RefreshSkinningNow()
{
	RebuildComponentSpace();
	PerformCPUSkinning(CurrentPose);
	FinalizeRenderState();
}

void USkeletalMeshComponent::EvaluatePose(float DeltaTime)
{
	(void)DeltaTime;
	// 현재는 Bind Pose를 유지
}

void USkeletalMeshComponent::UpdatePoseLocal(float DeltaTime)
{
	(void)DeltaTime;
	// EvaluatePose에서 LocalTransforms를 직접 갱신하는 현재 구조를 유지한다.
}

void USkeletalMeshComponent::RebuildComponentSpace()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;
	CurrentPose.RebuildComponentSpace(Bones);
}

void USkeletalMeshComponent::PerformCPUSkinning(const FSkeletonPose& Pose)
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
	const TArray<FNormalVertex>& BindVerts = MeshAsset->Vertices;
	const TArray<FSkinWeight>& SkinWeights = MeshAsset->SkinWeights;
	const TArray<FBoneInfo>& Bones = MeshAsset->Bones;
	const TArray<FMatrix>& ComponentSpaceTransforms = Pose.ComponentTransforms;
	const int32 VertexCount = static_cast<int32>(BindVerts.size());

	if (static_cast<int32>(SkinBuffer.size()) != VertexCount)
	{
		SkinBuffer.resize(VertexCount);
	}

	// CPU Skinning 흐름:
	// Reference Pose 정점
	// -> 영향 본 반복
	// -> (InverseBindPose * CurrentComponentSpace) 스키닝 행렬 적용
	// -> 가중치 누적
	// -> SkinBuffer에 현재 포즈 정점/노멀 기록
	for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const FNormalVertex& BindVertex = BindVerts[VertexIndex];
		const FSkinWeight& Weight = SkinWeights[VertexIndex];

		FVector SkinnedPos(0.0f, 0.0f, 0.0f);
		FVector SkinnedNormal(0.0f, 0.0f, 0.0f);
		float TotalWeight = 0.0f;

		for (int32 Inf = 0; Inf < MaxBoneInfluences; ++Inf)
		{
			const int32 BoneIdx = Weight.BoneIndices[Inf];
			const float W = Weight.BoneWeights[Inf];

			if (BoneIdx == InvalidBoneIndex || W <= 0.0f)
				continue;

			// 현재 코드의 행렬 곱 순서를 그대로 사용한다.
			// 수학 규약(row/column major)에 따라 의미가 달라질 수 있어 추후 검증 포인트다.
			const FMatrix SkinningMatrix = Bones[BoneIdx].InverseBindPose * ComponentSpaceTransforms[BoneIdx];

			SkinnedPos += SkinningMatrix.TransformPositionWithW(BindVertex.pos) * W;
			SkinnedNormal += SkinningMatrix.TransformVector(BindVertex.normal) * W;
			TotalWeight += W;
		}

		SkinBuffer[VertexIndex] = BindVertex;
		if (TotalWeight > 0.0f)
		{
			SkinBuffer[VertexIndex].pos = SkinnedPos;
			SkinBuffer[VertexIndex].normal = SkinnedNormal.Normalized();
		}
	}
}

void USkeletalMeshComponent::FinalizeRenderState()
{
	// 본/스키닝 결과 변경을 렌더러와 바운드 갱신 경로에 알린다.
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}
