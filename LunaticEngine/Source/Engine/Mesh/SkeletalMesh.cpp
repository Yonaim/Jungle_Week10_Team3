#include "Mesh/SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletalMesh, UObject)

USkeletalMesh::~USkeletalMesh()
{
	delete SkeletalMesh;
	SkeletalMesh = nullptr;
}

void USkeletalMesh::Serialize(FArchive& Ar)
{
	// 로딩 시점에 애셋 본체를 생성해 두고,
	// FSkeletalMesh가 정점/인덱스/스킨/본/섹션을 일괄 직렬화한다.
	if (Ar.IsLoading() && !SkeletalMesh)
	{
		SkeletalMesh = new FSkeletalMesh();
	}

	SkeletalMesh->Serialize(Ar);
	Ar << StaticMaterials;
}

bool USkeletalMesh::IsValid() const
{
	return SkeletalMesh != nullptr;
}

int32 USkeletalMesh::GetBoneCount() const
{
	return SkeletalMesh ? static_cast<int32>(SkeletalMesh->Bones.size()) : 0;
}

int32 USkeletalMesh::GetVertexCount(int32 LODIndex) const
{
	(void)LODIndex;
	return SkeletalMesh ? static_cast<int32>(SkeletalMesh->Vertices.size()) : 0;
}

int32 USkeletalMesh::GetIndexCount(int32 LODIndex) const
{
	(void)LODIndex;
	return SkeletalMesh ? static_cast<int32>(SkeletalMesh->Indices.size()) : 0;
}

void USkeletalMesh::SetSkeletalMeshAsset(FSkeletalMesh* InMesh)
{
	// 이전 애셋 메모리를 해제하고 새 Cooked Data로 교체한다.
	delete SkeletalMesh;
	SkeletalMesh = InMesh;
}

FSkeletalMesh* USkeletalMesh::GetSkeletalMeshAsset() const
{
	return SkeletalMesh;
}

void USkeletalMesh::SetStaticMaterials(TArray<FStaticMaterial>&& InMaterials)
{
	StaticMaterials = std::move(InMaterials);
}

const TArray<FStaticMaterial>& USkeletalMesh::GetStaticMaterials() const
{
	return StaticMaterials;
}
