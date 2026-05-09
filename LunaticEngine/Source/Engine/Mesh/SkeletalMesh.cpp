#include "Mesh/SkeletalMesh.h"

IMPLEMENT_CLASS(USkeletalMesh, UObject)

USkeletalMesh::~USkeletalMesh()
{
	delete SkeletalMesh;
	SkeletalMesh = nullptr;
}

void USkeletalMesh::Serialize(FArchive& Ar)
{
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
