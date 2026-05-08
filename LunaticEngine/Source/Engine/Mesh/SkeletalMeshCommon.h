#pragma once

#pragma once

#include "Core/CoreTypes.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

#include "Engine/Object/Object.h"
#include "Engine/Object/FName.h"
#include "Render/Resource/Buffer.h"
#include "Serialization/Archive.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

#include <memory>
#include <algorithm>

constexpr int32 MaxBoneInfluences = 4;
constexpr int32 InvalidBoneIndex = -1;

struct FSkeletalMeshVertex
{
	FVector Position;
	FVector Normal;
	FVector Tangent;
	FVector2 UV;
};

struct FSkinWeight
{
	int32 BoneIndices[MaxBoneInfluences] =
	{
		InvalidBoneIndex,
		InvalidBoneIndex,
		InvalidBoneIndex,
		InvalidBoneIndex
	};

	float BoneWeights[MaxBoneInfluences] =
	{
		0.0f,
		0.0f,
		0.0f,
		0.0f
	};
};

struct FBoneInfo
{
	std::string Name;

	// Root Bone이면 InvalidBoneIndex
	int32 ParentIndex = InvalidBoneIndex;

	// Bind Pose에서 Parent Bone 기준 Local Transform
	FTransform LocalBindTransform;

	// Bind Pose에서 Mesh Space 기준 Inverse Matrix
	// SkinningMatrix = CurrentBoneMatrix * InverseBindPose
	FMatrix InverseBindPose;
};

class USkeleton
{
public:
	std::vector<FBoneInfo> Bones;

public:
	int32 GetBoneCount() const;
	int32 FindBoneIndex(const std::string& BoneName) const;

	const FBoneInfo* GetBoneInfo(int32 BoneIndex) const;
	int32 GetParentBoneIndex(int32 BoneIndex) const;

	bool IsValidBoneIndex(int32 BoneIndex) const;
};

struct FSkeletalMeshRenderSection
{
	int32 MaterialIndex = 0;

	uint32 IndexStart = 0;
	uint32 IndexCount = 0;

	uint32 VertexStart = 0;
	uint32 VertexCount = 0;
};

struct FSkeletalMeshLOD
{
	std::vector<FSkeletalMeshVertex> Vertices;
	std::vector<uint32> Indices;
	std::vector<FSkinWeight> SkinWeights;

	std::vector<FSkeletalMeshRenderSection> Sections;
};