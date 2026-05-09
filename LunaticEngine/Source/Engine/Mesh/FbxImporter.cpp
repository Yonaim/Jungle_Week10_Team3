#include "Mesh/FbxImporter.h"
#include "FBXSDK/include/fbxsdk.h"

#include "Core/Log.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Mesh/StaticMeshCommon.h"
#include "Resource/ResourceManager.h"
#include "SimpleJSON/json.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace
{
	struct FFbxMaterialInfo
	{
		FbxSurfaceMaterial* SourceMaterial = nullptr;
		FString MaterialSlotName = "None";
		FString DiffuseTexturePath;
		FVector DiffuseColor = FVector(1.0f, 1.0f, 1.0f);
	};

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

	struct FFbxInfo
	{
		FbxManager* Manager = nullptr;
		FbxScene* Scene = nullptr;
		TArray<FFbxMaterialInfo> Materials;
		TArray<TArray<uint32>> FacesPerMaterial;
		std::unordered_map<FbxSurfaceMaterial*, int32> MaterialMap;
	};

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

	FString ConvertMaterialInfoToMat(const FFbxMaterialInfo& MaterialInfo)
	{
		if (!MaterialInfo.SourceMaterial)
		{
			return "None";
		}

		const FString AutoMaterialDirectory = FResourceManager::Get().ResolvePath(FName("Default.Directory.MaterialAuto"));
		const FString MatPath = AutoMaterialDirectory + "/" + MaterialInfo.MaterialSlotName + ".mat";

		if (std::filesystem::exists(FPaths::ToWide(MatPath)))
		{
			return MatPath;
		}

		std::filesystem::create_directories(FPaths::ToWide(AutoMaterialDirectory));

		json::JSON JsonData;
		JsonData["PathFileName"] = MatPath;
		JsonData["Origin"] = "FbxImport";
		JsonData["ShaderPath"] = "Shaders/Geometry/UberLit.hlsl";
		JsonData["RenderPass"] = "Opaque";

		if (!MaterialInfo.DiffuseTexturePath.empty())
		{
			JsonData["Textures"]["DiffuseTexture"] = MaterialInfo.DiffuseTexturePath;
			JsonData["Parameters"]["SectionColor"][0] = 1.0f;
			JsonData["Parameters"]["SectionColor"][1] = 1.0f;
			JsonData["Parameters"]["SectionColor"][2] = 1.0f;
			JsonData["Parameters"]["SectionColor"][3] = 1.0f;
		}
		else
		{
			JsonData["Parameters"]["SectionColor"][0] = MaterialInfo.DiffuseColor.X;
			JsonData["Parameters"]["SectionColor"][1] = MaterialInfo.DiffuseColor.Y;
			JsonData["Parameters"]["SectionColor"][2] = MaterialInfo.DiffuseColor.Z;
			JsonData["Parameters"]["SectionColor"][3] = 1.0f;
		}

		std::ofstream File(FPaths::ToWide(MatPath));
		File << JsonData.dump();
		return MatPath;
	}

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

	FVector RemapVector(const FbxVector4& Vector)
	{
		return FVector(
			static_cast<float>(-Vector[1]),
			static_cast<float>(-Vector[0]),
			static_cast<float>(Vector[2]));
	}

	FVector2 RemapUV(const FbxVector2& UV)
	{
		return FVector2(
			static_cast<float>(UV[0]),
			1.0f - static_cast<float>(UV[1]));
	}

	void BuildSections(FStaticMesh& OutMesh, const TArray<FStaticMaterial>& OutMaterials, const TArray<TArray<uint32>>& FacesPerMaterial)
	{
		TArray<uint32> OldIndices = std::move(OutMesh.Indices);
		OutMesh.Indices.clear();
		OutMesh.Sections.clear();

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

	void Destroy(FFbxInfo& FbxInfo)
	{
		if (FbxInfo.Manager)
		{
			FbxInfo.Manager->Destroy();
		}

		FbxInfo = FFbxInfo();
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

	int32 FindOrAddMaterial(const FString& FbxFilePath, FbxNode* Node, int32 LocalMaterialIndex, FFbxInfo& FbxInfo)
	{
		FbxSurfaceMaterial* SourceMaterial = Node ? Node->GetMaterial(LocalMaterialIndex) : nullptr;
		if (!SourceMaterial)
		{
			if (FbxInfo.Materials.empty())
			{
				FFbxMaterialInfo DefaultMaterial;
				FbxInfo.Materials.push_back(DefaultMaterial);
			}
			return 0;
		}

		auto It = FbxInfo.MaterialMap.find(SourceMaterial);
		if (It != FbxInfo.MaterialMap.end())
		{
			return It->second;
		}

		FFbxMaterialInfo MaterialInfo;
		MaterialInfo.SourceMaterial = SourceMaterial;
		MaterialInfo.MaterialSlotName = SanitizeMaterialName(SourceMaterial->GetName());
		MaterialInfo.DiffuseTexturePath = FindDiffuseTexturePath(FbxFilePath, SourceMaterial);
		MaterialInfo.DiffuseColor = GetDiffuseColor(SourceMaterial);

		const int32 MaterialIndex = static_cast<int32>(FbxInfo.Materials.size());
		FbxInfo.Materials.push_back(MaterialInfo);
		FbxInfo.MaterialMap.emplace(SourceMaterial, MaterialIndex);
		return MaterialIndex;
	}

	bool ConvertMeshNode(const FString& FbxFilePath, FbxNode* Node, FFbxInfo& FbxInfo, FStaticMesh& OutMesh)
	{
		FbxMesh* Mesh = Node ? Node->GetMesh() : nullptr;
		if (!Mesh)
		{
			return true;
		}

		// UV
		const char* UVSetName = nullptr;
		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		if (UVSetNames.GetCount() > 0)
		{
			UVSetName = UVSetNames[0];
		}

		// World Matrix
		const FbxAMatrix GlobalTransform = Node->EvaluateGlobalTransform();
		std::unordered_map<FFbxVertexKey, uint32, FFbxVertexKeyHash> VertexMap;
		const int32 PolygonCount = Mesh->GetPolygonCount();

		// Polygon(Triangle) 순회
		for (int32 PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
		{
			if (Mesh->GetPolygonSize(PolygonIndex) != 3)
			{
				continue;
			}

			// Material Mapping
			int32 LocalMaterialIndex = GetPolygonMaterialIndex(Mesh, PolygonIndex);
			if (LocalMaterialIndex < 0)
			{
				LocalMaterialIndex = 0;
			}

			const int32 MaterialIndex = FindOrAddMaterial(FbxFilePath, Node, LocalMaterialIndex, FbxInfo);
			if (static_cast<size_t>(MaterialIndex) >= FbxInfo.FacesPerMaterial.size())
			{
				FbxInfo.FacesPerMaterial.resize(static_cast<size_t>(MaterialIndex) + 1);
			}

			// Vertex Data
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
				Vertex.pos = RemapVector(GlobalTransform.MultT(Mesh->GetControlPointAt(ControlPointIndex)));

				FbxVector4 FbxNormal(0.0, 0.0, 1.0, 0.0);
				Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, FbxNormal);
				Vertex.normal = RemapVector(GlobalTransform.MultR(FbxNormal)).Normalized();

				FbxVector2 FbxUV(0.0, 0.0);
				bool bUnmapped = false;
				Vertex.tex = UVSetName && Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetName, FbxUV, bUnmapped) && !bUnmapped
					? RemapUV(FbxUV)
					: FVector2(0.0f, 0.0f);
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
			FbxInfo.FacesPerMaterial[MaterialIndex].push_back(FaceStart);
		}

		return true;
	}

	void TraverseNodes(const FString& FbxFilePath, FbxNode* Node, FFbxInfo& FbxInfo, FStaticMesh& OutMesh)
	{
		if (!Node)
		{
			return;
		}

		ConvertMeshNode(FbxFilePath, Node, FbxInfo, OutMesh);

		const int32 ChildCount = Node->GetChildCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			TraverseNodes(FbxFilePath, Node->GetChild(ChildIndex), FbxInfo, OutMesh);
		}
	}

	bool ParseFbx(const FString& FbxFilePath, FFbxInfo& OutFbxInfo)
	{
		OutFbxInfo.Manager = FbxManager::Create();
		if (!OutFbxInfo.Manager)
		{
			UE_LOG_CATEGORY(ObjImporter, Error, "Failed to create FBX SDK manager.");
			return false;
		}

		FbxIOSettings* IOSettings = FbxIOSettings::Create(OutFbxInfo.Manager, IOSROOT);
		OutFbxInfo.Manager->SetIOSettings(IOSettings);

		FbxImporter* Importer = FbxImporter::Create(OutFbxInfo.Manager, "");
		if (!Importer->Initialize(FbxFilePath.c_str(), -1, OutFbxInfo.Manager->GetIOSettings()))
		{
			UE_LOG_CATEGORY(ObjImporter, Error, "Failed to initialize FBX importer: %s", FbxFilePath.c_str());
			Importer->Destroy();
			Destroy(OutFbxInfo);
			return false;
		}

		OutFbxInfo.Scene = FbxScene::Create(OutFbxInfo.Manager, "ImportedFbxScene");
		if (!Importer->Import(OutFbxInfo.Scene))
		{
			UE_LOG_CATEGORY(ObjImporter, Error, "Failed to import FBX scene: %s", FbxFilePath.c_str());
			Importer->Destroy();
			Destroy(OutFbxInfo);
			return false;
		}

		Importer->Destroy();

		FbxGeometryConverter GeometryConverter(OutFbxInfo.Manager);
		GeometryConverter.Triangulate(OutFbxInfo.Scene, true);
		return true;
	}

	bool Convert(const FString& FbxFilePath, FFbxInfo& FbxInfo, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
	{
		OutMesh = FStaticMesh();
		OutMaterials.clear();

		TraverseNodes(FbxFilePath, FbxInfo.Scene ? FbxInfo.Scene->GetRootNode() : nullptr, FbxInfo, OutMesh);

		if (FbxInfo.Materials.empty() && !OutMesh.Indices.empty())
		{
			FFbxMaterialInfo DefaultMaterial;
			FbxInfo.Materials.push_back(DefaultMaterial);
			FbxInfo.FacesPerMaterial.resize(1);
			for (size_t FaceStart = 0; FaceStart + 2 < OutMesh.Indices.size(); FaceStart += 3)
			{
				FbxInfo.FacesPerMaterial[0].push_back(static_cast<uint32>(FaceStart));
			}
		}

		for (const FFbxMaterialInfo& MaterialInfo : FbxInfo.Materials)
		{
			FStaticMaterial StaticMaterial;
			StaticMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(ConvertMaterialInfoToMat(MaterialInfo));
			StaticMaterial.MaterialSlotName = MaterialInfo.MaterialSlotName;
			OutMaterials.push_back(StaticMaterial);
		}

		BuildSections(OutMesh, OutMaterials, FbxInfo.FacesPerMaterial);
		OutMesh.PathFileName = FbxFilePath;
		OutMesh.CacheBounds();
		AccumulateTangents(OutMesh);

		return !OutMesh.Vertices.empty() && !OutMesh.Indices.empty();
	}
}

bool FFbxImporter::Import(const FString& FbxFilePath, FStaticMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	//auto StartTime = std::chrono::high_resolution_clock::now();

	FFbxInfo FbxInfo;
	
	// FBX Parsing (Raw Data)
	if (!ParseFbx(FbxFilePath, FbxInfo))
	{
		return false;
	}

	// Cooked Data
	const bool bImported = Convert(FbxFilePath, FbxInfo, OutMesh, OutMaterials);
	Destroy(FbxInfo);

	//auto EndTime = std::chrono::high_resolution_clock::now();
	//std::chrono::duration<double> Duration = EndTime - StartTime;
	//UE_LOG_CATEGORY(ObjImporter, Info, "FBX Imported %s. File: %s. Time taken: %.4f seconds",
	//	bImported ? "successfully" : "with no mesh data", FbxFilePath.c_str(), Duration.count());

	return bImported;
}
