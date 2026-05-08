#include "Mesh/FbxImporter.h"

#include "Core/Log.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Mesh/StaticMeshCommon.h"
#include "Resource/ResourceManager.h"
#include "SimpleJSON/json.hpp"

#if defined(_WIN64)
#include "FBXSDK/include/fbxsdk.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace
{
	struct FFbxVertexKey
	{
		int32 ControlPointIndex = -1;
		int32 PolygonIndex = -1;
		int32 CornerIndex = -1;
		int32 MaterialIndex = 0;

		bool operator==(const FFbxVertexKey& Other) const
		{
			return ControlPointIndex == Other.ControlPointIndex
				&& PolygonIndex == Other.PolygonIndex
				&& CornerIndex == Other.CornerIndex
				&& MaterialIndex == Other.MaterialIndex;
		}
	};

	struct FFbxVertexKeyHash
	{
		size_t operator()(const FFbxVertexKey& Key) const noexcept
		{
			size_t Seed = std::hash<int32>{}(Key.ControlPointIndex);
			Seed ^= std::hash<int32>{}(Key.PolygonIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
			Seed ^= std::hash<int32>{}(Key.CornerIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
			Seed ^= std::hash<int32>{}(Key.MaterialIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
			return Seed;
		}
	};

	FString SanitizeMaterialName(FString Name)
	{
		if (Name.empty())
		{
			return "None";
		}

		for (char& Character : Name)
		{
			const bool bAlphaNum = (Character >= 'a' && Character <= 'z')
				|| (Character >= 'A' && Character <= 'Z')
				|| (Character >= '0' && Character <= '9');
			if (!bAlphaNum && Character != '_' && Character != '-')
			{
				Character = '_';
			}
		}

		return Name;
	}

	FVector RemapFbxVector(const FbxVector4& Vector)
	{
		return FVector(
			static_cast<float>(-Vector[1]),
			static_cast<float>(-Vector[0]),
			static_cast<float>(Vector[2]));
	}

	FVector2 RemapFbxUV(const FbxVector2& UV)
	{
		return FVector2(
			static_cast<float>(UV[0]),
			1.0f - static_cast<float>(UV[1]));
	}

	FString MakeProjectRelativePath(const std::filesystem::path& Path)
	{
		const std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir()).lexically_normal();
		const std::filesystem::path NormalizedPath = Path.lexically_normal();
		std::error_code ErrorCode;
		const std::filesystem::path RelativePath = std::filesystem::relative(NormalizedPath, RootPath, ErrorCode);
		if (!ErrorCode && !RelativePath.empty())
		{
			return FPaths::ToUtf8(RelativePath.generic_wstring());
		}
		return FPaths::ToUtf8(NormalizedPath.generic_wstring());
	}

	FString ResolveFbxTexturePath(const FString& FbxFilePath, FbxFileTexture* Texture)
	{
		if (!Texture)
		{
			return "";
		}

		FString TexturePath = Texture->GetRelativeFileName();
		if (TexturePath.empty())
		{
			TexturePath = Texture->GetFileName();
		}
		if (TexturePath.empty())
		{
			return "";
		}

		std::filesystem::path Candidate(FPaths::ToWide(TexturePath));
		if (!Candidate.is_absolute())
		{
			Candidate = std::filesystem::path(FPaths::ToWide(FbxFilePath)).parent_path() / Candidate;
		}

		return MakeProjectRelativePath(Candidate);
	}

	FString FindDiffuseTexturePath(const FString& FbxFilePath, FbxSurfaceMaterial* FbxMaterial)
	{
		if (!FbxMaterial)
		{
			return "";
		}

		FbxProperty DiffuseProperty = FbxMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
		if (!DiffuseProperty.IsValid())
		{
			return "";
		}

		const int32 TextureCount = DiffuseProperty.GetSrcObjectCount<FbxFileTexture>();
		if (TextureCount <= 0)
		{
			return "";
		}

		return ResolveFbxTexturePath(FbxFilePath, DiffuseProperty.GetSrcObject<FbxFileTexture>(0));
	}

	FVector GetDiffuseColor(FbxSurfaceMaterial* FbxMaterial)
	{
		if (FbxSurfaceLambert* Lambert = FbxCast<FbxSurfaceLambert>(FbxMaterial))
		{
			const FbxDouble3 Diffuse = Lambert->Diffuse.Get();
			const double DiffuseFactor = Lambert->DiffuseFactor.Get();
			return FVector(
				static_cast<float>(Diffuse[0] * DiffuseFactor),
				static_cast<float>(Diffuse[1] * DiffuseFactor),
				static_cast<float>(Diffuse[2] * DiffuseFactor));
		}

		return FVector(1.0f, 1.0f, 1.0f);
	}

	UMaterial* CreateOrLoadFbxMaterial(const FString& FbxFilePath, FbxSurfaceMaterial* FbxMaterial, FString& OutSlotName)
	{
		OutSlotName = FbxMaterial ? SanitizeMaterialName(FbxMaterial->GetName()) : "None";

		const FString AutoMaterialDirectory = FResourceManager::Get().ResolvePath(FName("Default.Directory.MaterialAuto"));
		const FString MatPath = AutoMaterialDirectory + "/" + OutSlotName + ".mat";

		if (!std::filesystem::exists(FPaths::ToWide(MatPath)))
		{
			std::filesystem::create_directories(FPaths::ToWide(AutoMaterialDirectory));

			json::JSON JsonData;
			JsonData["PathFileName"] = MatPath;
			JsonData["Origin"] = "FbxImport";
			JsonData["ShaderPath"] = "Shaders/Geometry/UberLit.hlsl";
			JsonData["RenderPass"] = "Opaque";

			const FString DiffuseTexturePath = FindDiffuseTexturePath(FbxFilePath, FbxMaterial);
			if (!DiffuseTexturePath.empty())
			{
				JsonData["Textures"]["DiffuseTexture"] = DiffuseTexturePath;
				JsonData["Parameters"]["SectionColor"][0] = 1.0f;
				JsonData["Parameters"]["SectionColor"][1] = 1.0f;
				JsonData["Parameters"]["SectionColor"][2] = 1.0f;
				JsonData["Parameters"]["SectionColor"][3] = 1.0f;
			}
			else
			{
				const FVector DiffuseColor = GetDiffuseColor(FbxMaterial);
				JsonData["Parameters"]["SectionColor"][0] = DiffuseColor.X;
				JsonData["Parameters"]["SectionColor"][1] = DiffuseColor.Y;
				JsonData["Parameters"]["SectionColor"][2] = DiffuseColor.Z;
				JsonData["Parameters"]["SectionColor"][3] = 1.0f;
			}

			std::ofstream File(FPaths::ToWide(MatPath));
			File << JsonData.dump();
		}

		return FMaterialManager::Get().GetOrCreateMaterial(MatPath);
	}

	int32 FindOrAddMaterialSlot(
		const FString& FbxFilePath,
		FbxSurfaceMaterial* FbxMaterial,
		TArray<FStaticMaterial>& OutMaterials,
		std::unordered_map<FbxSurfaceMaterial*, int32>& MaterialMap)
	{
		if (!FbxMaterial)
		{
			if (OutMaterials.empty())
			{
				FStaticMaterial DefaultMaterial;
				DefaultMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");
				DefaultMaterial.MaterialSlotName = "None";
				OutMaterials.push_back(DefaultMaterial);
			}
			return 0;
		}

		auto It = MaterialMap.find(FbxMaterial);
		if (It != MaterialMap.end())
		{
			return It->second;
		}

		FString SlotName;
		FStaticMaterial StaticMaterial;
		StaticMaterial.MaterialInterface = CreateOrLoadFbxMaterial(FbxFilePath, FbxMaterial, SlotName);
		StaticMaterial.MaterialSlotName = SlotName;

		const int32 MaterialIndex = static_cast<int32>(OutMaterials.size());
		OutMaterials.push_back(StaticMaterial);
		MaterialMap.emplace(FbxMaterial, MaterialIndex);
		return MaterialIndex;
	}

	int32 GetPolygonMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
	{
		FbxLayerElementMaterial* MaterialElement = Mesh->GetElementMaterial();
		if (!MaterialElement)
		{
			return 0;
		}

		if (MaterialElement->GetMappingMode() == FbxLayerElement::eAllSame)
		{
			return MaterialElement->GetIndexArray().GetCount() > 0 ? MaterialElement->GetIndexArray().GetAt(0) : 0;
		}

		if (MaterialElement->GetMappingMode() == FbxLayerElement::eByPolygon)
		{
			return MaterialElement->GetIndexArray().GetCount() > PolygonIndex ? MaterialElement->GetIndexArray().GetAt(PolygonIndex) : 0;
		}

		return 0;
	}

	void AccumulateTangents(FStaticMesh& Mesh)
	{
		TArray<FVector> TangentSums(Mesh.Vertices.size(), FVector(0.0f, 0.0f, 0.0f));
		TArray<FVector> BitangentSums(Mesh.Vertices.size(), FVector(0.0f, 0.0f, 0.0f));

		for (size_t i = 0; i + 2 < Mesh.Indices.size(); i += 3)
		{
			const uint32 I0 = Mesh.Indices[i + 0];
			const uint32 I1 = Mesh.Indices[i + 1];
			const uint32 I2 = Mesh.Indices[i + 2];

			const FNormalVertex& V0 = Mesh.Vertices[I0];
			const FNormalVertex& V1 = Mesh.Vertices[I1];
			const FNormalVertex& V2 = Mesh.Vertices[I2];

			const FVector Edge1 = V1.pos - V0.pos;
			const FVector Edge2 = V2.pos - V0.pos;
			const FVector2 DeltaUV1 = V1.tex - V0.tex;
			const FVector2 DeltaUV2 = V2.tex - V0.tex;

			const float Det = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
			if (std::abs(Det) < 1e-8f)
			{
				continue;
			}

			const float InvDet = 1.0f / Det;
			const FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDet;
			const FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * InvDet;

			TangentSums[I0] += Tangent;
			TangentSums[I1] += Tangent;
			TangentSums[I2] += Tangent;
			BitangentSums[I0] += Bitangent;
			BitangentSums[I1] += Bitangent;
			BitangentSums[I2] += Bitangent;
		}

		for (size_t i = 0; i < Mesh.Vertices.size(); ++i)
		{
			FNormalVertex& Vertex = Mesh.Vertices[i];
			FVector Normal = Vertex.normal.Normalized();
			FVector Tangent = TangentSums[i];
			Tangent = (Tangent - Normal * Normal.Dot(Tangent)).Normalized();
			if (Tangent.IsNearlyZero())
			{
				Tangent = FVector(1.0f, 0.0f, 0.0f);
			}

			const float Handedness = Normal.Cross(Tangent).Dot(BitangentSums[i]) < 0.0f ? -1.0f : 1.0f;
			Vertex.tangent = FVector4(Tangent, Handedness);
		}
	}

	bool ImportMeshNode(
		const FString& FbxFilePath,
		FbxNode* Node,
		FStaticMesh& OutMesh,
		TArray<FStaticMaterial>& OutMaterials,
		TArray<TArray<uint32>>& FacesPerMaterial,
		std::unordered_map<FbxSurfaceMaterial*, int32>& MaterialMap)
	{
		FbxMesh* Mesh = Node ? Node->GetMesh() : nullptr;
		if (!Mesh)
		{
			return true;
		}

		const char* UVSetName = nullptr;
		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		if (UVSetNames.GetCount() > 0)
		{
			UVSetName = UVSetNames[0];
		}

		const FbxAMatrix GlobalTransform = Node->EvaluateGlobalTransform();
		std::unordered_map<FFbxVertexKey, uint32, FFbxVertexKeyHash> VertexMap;
		const int32 PolygonCount = Mesh->GetPolygonCount();

		for (int32 PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
		{
			if (Mesh->GetPolygonSize(PolygonIndex) != 3)
			{
				continue;
			}

			int32 LocalMaterialIndex = GetPolygonMaterialIndex(Mesh, PolygonIndex);
			if (LocalMaterialIndex < 0)
			{
				LocalMaterialIndex = 0;
			}

			const int32 MaterialIndex = FindOrAddMaterialSlot(
				FbxFilePath,
				Node->GetMaterial(LocalMaterialIndex),
				OutMaterials,
				MaterialMap);

			if (static_cast<size_t>(MaterialIndex) >= FacesPerMaterial.size())
			{
				FacesPerMaterial.resize(static_cast<size_t>(MaterialIndex) + 1);
			}

			uint32 TriangleIndices[3] = {};
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const int32 ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);
				const FFbxVertexKey Key{ ControlPointIndex, PolygonIndex, CornerIndex, MaterialIndex };

				auto It = VertexMap.find(Key);
				if (It != VertexMap.end())
				{
					TriangleIndices[CornerIndex] = It->second;
					continue;
				}

				FNormalVertex Vertex;
				const FbxVector4 FbxPosition = GlobalTransform.MultT(Mesh->GetControlPointAt(ControlPointIndex));
				Vertex.pos = RemapFbxVector(FbxPosition);

				FbxVector4 FbxNormal(0.0, 0.0, 1.0, 0.0);
				Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, FbxNormal);
				FbxNormal = GlobalTransform.MultR(FbxNormal);
				Vertex.normal = RemapFbxVector(FbxNormal).Normalized();

				FbxVector2 FbxUV(0.0, 0.0);
				bool bUnmapped = false;
				if (UVSetName && Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetName, FbxUV, bUnmapped) && !bUnmapped)
				{
					Vertex.tex = RemapFbxUV(FbxUV);
				}
				else
				{
					Vertex.tex = FVector2(0.0f, 0.0f);
				}

				Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

				const uint32 NewVertexIndex = static_cast<uint32>(OutMesh.Vertices.size());
				OutMesh.Vertices.push_back(Vertex);
				VertexMap.emplace(Key, NewVertexIndex);
				TriangleIndices[CornerIndex] = NewVertexIndex;
			}

			const uint32 FaceStart = static_cast<uint32>(OutMesh.Indices.size());
			OutMesh.Indices.push_back(TriangleIndices[0]);
			OutMesh.Indices.push_back(TriangleIndices[2]);
			OutMesh.Indices.push_back(TriangleIndices[1]);
			FacesPerMaterial[MaterialIndex].push_back(FaceStart);
		}

		return true;
	}

	void TraverseNodes(
		const FString& FbxFilePath,
		FbxNode* Node,
		FStaticMesh& OutMesh,
		TArray<FStaticMaterial>& OutMaterials,
		TArray<TArray<uint32>>& FacesPerMaterial,
		std::unordered_map<FbxSurfaceMaterial*, int32>& MaterialMap)
	{
		if (!Node)
		{
			return;
		}

		ImportMeshNode(FbxFilePath, Node, OutMesh, OutMaterials, FacesPerMaterial, MaterialMap);

		const int32 ChildCount = Node->GetChildCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			TraverseNodes(FbxFilePath, Node->GetChild(ChildIndex), OutMesh, OutMaterials, FacesPerMaterial, MaterialMap);
		}
	}
}

bool FFbxImporter::Import(const FString& FbxFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	auto StartTime = std::chrono::high_resolution_clock::now();

	OutMesh = FStaticMesh();
	OutMaterials.clear();

	FbxManager* Manager = FbxManager::Create();
	if (!Manager)
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to create FBX SDK manager.");
		return false;
	}

	FbxIOSettings* IOSettings = FbxIOSettings::Create(Manager, IOSROOT);
	Manager->SetIOSettings(IOSettings);

	FbxImporter* Importer = FbxImporter::Create(Manager, "");
	if (!Importer->Initialize(FbxFilePath.c_str(), -1, Manager->GetIOSettings()))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to initialize FBX importer: %s", FbxFilePath.c_str());
		Manager->Destroy();
		return false;
	}

	FbxScene* Scene = FbxScene::Create(Manager, "ImportedFbxScene");
	if (!Importer->Import(Scene))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to import FBX scene: %s", FbxFilePath.c_str());
		Importer->Destroy();
		Manager->Destroy();
		return false;
	}
	Importer->Destroy();

	FbxGeometryConverter GeometryConverter(Manager);
	GeometryConverter.Triangulate(Scene, true);

	FbxNode* RootNode = Scene->GetRootNode();
	TArray<TArray<uint32>> FacesPerMaterial;
	std::unordered_map<FbxSurfaceMaterial*, int32> MaterialMap;
	TraverseNodes(FbxFilePath, RootNode, OutMesh, OutMaterials, FacesPerMaterial, MaterialMap);

	if (OutMaterials.empty() && !OutMesh.Indices.empty())
	{
		FStaticMaterial DefaultMaterial;
		DefaultMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial("None");
		DefaultMaterial.MaterialSlotName = "None";
		OutMaterials.push_back(DefaultMaterial);
		FacesPerMaterial.resize(1);
		for (size_t FaceStart = 0; FaceStart + 2 < OutMesh.Indices.size(); FaceStart += 3)
		{
			FacesPerMaterial[0].push_back(static_cast<uint32>(FaceStart));
		}
	}

	TArray<uint32> OldIndices = std::move(OutMesh.Indices);
	OutMesh.Indices.clear();
	for (size_t MaterialIndex = 0; MaterialIndex < FacesPerMaterial.size() && MaterialIndex < OutMaterials.size(); ++MaterialIndex)
	{
		const TArray<uint32>& FaceStarts = FacesPerMaterial[MaterialIndex];
		if (FaceStarts.empty())
		{
			continue;
		}

		FStaticMeshSection Section;
		Section.MaterialIndex = static_cast<int32>(MaterialIndex);
		Section.MaterialSlotName = OutMaterials[MaterialIndex].MaterialSlotName;
		Section.FirstIndex = static_cast<uint32>(OutMesh.Indices.size());
		Section.NumTriangles = static_cast<uint32>(FaceStarts.size());

		for (uint32 FaceStart : FaceStarts)
		{
			OutMesh.Indices.push_back(OldIndices[FaceStart + 0]);
			OutMesh.Indices.push_back(OldIndices[FaceStart + 1]);
			OutMesh.Indices.push_back(OldIndices[FaceStart + 2]);
		}

		OutMesh.Sections.push_back(Section);
	}

	OutMesh.PathFileName = FbxFilePath;
	OutMesh.CacheBounds();
	AccumulateTangents(OutMesh);

	const bool bImported = !OutMesh.Vertices.empty() && !OutMesh.Indices.empty();
	Manager->Destroy();

	auto EndTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> Duration = EndTime - StartTime;
	UE_LOG_CATEGORY(ObjImporter, Info, "FBX Imported %s. File: %s. Time taken: %.4f seconds",
		bImported ? "successfully" : "with no mesh data", FbxFilePath.c_str(), Duration.count());

	return bImported;
}
#else
bool FFbxImporter::Import(const FString& FbxFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	(void)FbxFilePath;
	OutMesh = FStaticMesh();
	OutMaterials.clear();
	UE_LOG_CATEGORY(ObjImporter, Error, "FBX import requires the x64 FBX SDK build.");
	return false;
}
#endif
