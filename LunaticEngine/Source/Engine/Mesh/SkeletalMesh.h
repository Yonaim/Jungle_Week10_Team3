#pragma once

#include "Object/Object.h"
#include "Engine/Mesh/SkeletalMeshCommon.h"

class USkeletalMesh
{
public:
	USkeleton* Skeleton = nullptr;

	// 기본 과제에서는 LOD[0]만 사용
	std::vector<FSkeletalMeshLOD> LODs;

public:
	bool IsValid() const;

	const FSkeletalMeshLOD* GetLOD(int32 LODIndex = 0) const;
	FSkeletalMeshLOD* GetMutableLOD(int32 LODIndex = 0);

	int32 GetBoneCount() const;
	int32 GetVertexCount(int32 LODIndex = 0) const;
	int32 GetIndexCount(int32 LODIndex = 0) const;
};
