#include "SkinnedMeshComponent.h"
#include "Render/Proxy/SkeletalMeshProxy.h"
#include "Mesh/ObjManager.h"
#include "Engine/Runtime/Engine.h"
#include "Serialization/Archive.h"
#include <algorithm>

IMPLEMENT_CLASS(USkinnedMeshComponent, UMeshComponent)

void USkinnedMeshComponent::SetSkeletalMesh(USkeletalMesh* Mesh)
{
	// 컴포넌트는 애셋 경로를 직렬화용 식별자로만 저장한다.
	// 실제 지오메트리/스킨 데이터 소유권은 USkeletalMesh 내부에 있다.
	SkeletalMesh = Mesh;
	if (Mesh && Mesh->GetSkeletalMeshAsset())
	{
		SkeletalMeshPath = Mesh->GetSkeletalMeshAsset()->PathFileName;
	}
	else
	{
		SkeletalMeshPath = "None";
	}
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

USkeletalMesh* USkinnedMeshComponent::GetSkeletalMesh() const
{
	return SkeletalMesh;
}

FTransform USkinnedMeshComponent::GetBoneWorldTransform(int32 BoneIndex) const
{
	const TArray<FMatrix>& ComponentSpaceTransforms = CurrentPose.ComponentTransforms;
	if (BoneIndex < 0 || BoneIndex >= (int32)ComponentSpaceTransforms.size())
		return FTransform();

	// 본의 컴포넌트 공간 행렬에 컴포넌트 월드 행렬을 곱해
	// 최종 월드 공간 본 변환으로 변환한다.
	FMatrix BoneWorldMatrix = ComponentSpaceTransforms[BoneIndex] * GetWorldMatrix();
	return FTransform(
		BoneWorldMatrix.GetLocation(),
		BoneWorldMatrix.ToQuat(),
		BoneWorldMatrix.GetScale()
	);
}

FTransform USkinnedMeshComponent::GetBoneWorldTransformByName(const FString& BoneName) const
{
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		return FTransform();
	}

	const TArray<FBoneInfo>& Bones = SkeletalMesh->GetSkeletalMeshAsset()->Bones;
	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
	{
		if (Bones[BoneIndex].Name == BoneName)
		{
			return GetBoneWorldTransform(BoneIndex);
		}
	}

	return FTransform();
}

int32 USkinnedMeshComponent::GetBoneCount() const
{
	return (int32)CurrentPose.LocalTransforms.size();
}

FPrimitiveSceneProxy* USkinnedMeshComponent::CreateSceneProxy()
{
	// 렌더 제출 경계:
	// 컴포넌트 런타임 상태(CurrentPose/SkinBuffer)는 프록시가 소비하고,
	// DrawCommandBuilder는 프록시의 버퍼/섹션 정보만 사용한다.
	return new FSkeletalMeshProxy(this);
}

void USkinnedMeshComponent::UpdateWorldAABB() const
{
	// 스키닝 메시는 포즈에 따라 정점 위치가 매 프레임 변하므로,
	// 정적 애셋 바운드 대신 CPU Skinning 결과 버퍼로 AABB를 계산한다.
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
	if (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())
	{
		CurrentPose.Reset();
		return;
	}

	CurrentPose.InitializeFromBindPose(*SkeletalMesh->GetSkeletalMeshAsset());
}

void USkinnedMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });
}

void USkinnedMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		if (SkeletalMeshPath.empty() || SkeletalMeshPath == "None")
		{
			SetSkeletalMesh(nullptr);
		}
		else
		{
			ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
			USkeletalMesh* Loaded = FObjManager::LoadObjSkeletalMesh(SkeletalMeshPath, Device);
			SetSkeletalMesh(Loaded);
		}
	}
}

void USkinnedMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	Ar << SkeletalMeshPath;
}

void USkinnedMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	if (!SkeletalMeshPath.empty() && SkeletalMeshPath != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		USkeletalMesh* Loaded = FObjManager::LoadObjSkeletalMesh(SkeletalMeshPath, Device);
		SetSkeletalMesh(Loaded);
	}
}
