#pragma once

#pragma once

#include "Core/CoreTypes.h"
#include "Mesh/StaticMeshCommon.h"
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

struct FSkeletalMeshSection
{
	int32 MaterialIndex = 0;

	uint32 IndexStart = 0;
	uint32 IndexCount = 0;

	uint32 VertexStart = 0;
	uint32 VertexCount = 0;
};

// Cooked Data
struct FSkeletalMesh
{
	FString PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FSkinWeight> SkinWeights;

	TArray<FSkeletalMeshSection> Sections;

	void Serialize(FArchive& Ar)
	{
		Ar << PathFileName;
		Ar << Vertices;
		Ar << Indices;
		Ar << Sections;
	}
};
