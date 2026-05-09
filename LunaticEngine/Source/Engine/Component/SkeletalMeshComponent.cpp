#include "SkeletalMeshComponent.h"
#include "Serialization/Archive.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshCommon.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& LocalTransform)
{
	if (BoneIndex >= 0 && BoneIndex < (int32)BoneSpaceTransforms.size())
	{
		BoneSpaceTransforms[BoneIndex] = LocalTransform;
	}
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
	const int32 BoneCount = (int32)MeshAsset->Bones.size();

	BoneSpaceTransforms.resize(BoneCount);
	ComponentSpaceTransforms.resize(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i)
	{
		BoneSpaceTransforms[i] = MeshAsset->Bones[i].LocalBindTransform;
	}

	SkinBuffer = MeshAsset->Vertices;

	FillComponentSpaceTransforms();
	PerformCPUSkinning();
	FinalizeBoneTransforms();
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
	FillComponentSpaceTransforms();
	PerformCPUSkinning();
	FinalizeBoneTransforms();
}

void USkeletalMeshComponent::EvaluatePose(float DeltaTime)
{
	// 현재는 Bind Pose를 유지
}

void USkeletalMeshComponent::FillComponentSpaceTransforms()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;
	const int32 BoneCount = (int32)Bones.size();

	for (int32 i = 0; i < BoneCount; ++i)
	{
		const FMatrix LocalMatrix = BoneSpaceTransforms[i].ToMatrix();
		const int32 ParentIndex = Bones[i].ParentIndex;

		if (ParentIndex == InvalidBoneIndex)
		{
			ComponentSpaceTransforms[i] = LocalMatrix;
		}
		else
		{
			ComponentSpaceTransforms[i] = LocalMatrix * ComponentSpaceTransforms[ParentIndex];
		}
	}
}

void USkeletalMeshComponent::PerformCPUSkinning()
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
		return;

	const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
	const TArray<FNormalVertex>& BindVerts = MeshAsset->Vertices;
	const TArray<FSkinWeight>& SkinWeights = MeshAsset->SkinWeights;
	const TArray<FBoneInfo>& Bones = MeshAsset->Bones;
	const int32 VertexCount = (int32)BindVerts.size();

	if ((int32)SkinBuffer.size() != VertexCount)
	{
		SkinBuffer.resize(VertexCount);
	}

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

void USkeletalMeshComponent::FinalizeBoneTransforms()
{
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}
