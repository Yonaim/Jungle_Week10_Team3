#pragma once

#include "Core/CoreTypes.h"

struct FStaticMaterial;
struct FStaticMesh;

struct FFbxImporter
{
	static bool Import(const FString& FbxFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials);
};
