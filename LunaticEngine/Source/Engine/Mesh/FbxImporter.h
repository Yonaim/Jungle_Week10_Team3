#pragma once

#include "Core/CoreTypes.h"

struct FStaticMaterial;
struct FStaticMesh;
struct FSkeletalMesh;

struct FFbxImporter
{
	static bool Import(const FString& ObjFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
	static bool Import(const FString& FbxFilePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};
