#pragma once

#include "Object/Object.h"
#include "Engine/Mesh/SkeletalMeshCommon.h"

class USkeletalMesh : public UObject
{
public:
	DECLARE_CLASS(USkeletalMesh, UObject)

	USkeletalMesh() = default;
	~USkeletalMesh() override;

	bool IsValid() const;

	int32 GetBoneCount() const;
	int32 GetVertexCount(int32 LODIndex = 0) const;
	int32 GetIndexCount(int32 LODIndex = 0) const;

private:
	FSkeletalMesh* SkeletalMesh = nullptr;
};
