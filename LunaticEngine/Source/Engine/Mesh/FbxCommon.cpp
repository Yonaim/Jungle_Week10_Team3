#include "PCH/LunaticPCH.h"
#include "Mesh/FbxCommon.h"
#include "Materials/MaterialManager.h"

#include "Core/Log.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Core/SimpleJsonWrapper.h"

#include <fstream>
#include <filesystem>
#include <cmath>

#if defined(_WIN64)

namespace
{
	int32 GetPolygonVertexLinearIndex(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex)
	{
		if (!Mesh || PolygonIndex < 0 || CornerIndex < 0)
		{
			return -1;
		}

		int32 LinearIndex = 0;
		for (int32 Index = 0; Index < PolygonIndex; ++Index)
		{
			LinearIndex += Mesh->GetPolygonSize(Index);
		}
		return LinearIndex + CornerIndex;
	}

	int32 ResolveLayerElementIndex(const FbxLayerElementTemplate<FbxVector4>* Element, int32 ElementIndex)
	{
		if (!Element || ElementIndex < 0)
		{
			return -1;
		}

		switch (Element->GetReferenceMode())
		{
		case FbxLayerElement::eDirect:
			return ElementIndex;
		case FbxLayerElement::eIndexToDirect:
			return ElementIndex < Element->GetIndexArray().GetCount() ? Element->GetIndexArray().GetAt(ElementIndex) : -1;
		default:
			return -1;
		}
	}

	bool IsFiniteVector(const FVector& Vector)
	{
		return std::isfinite(Vector.X) && std::isfinite(Vector.Y) && std::isfinite(Vector.Z);
	}
}

size_t FFbxVertexKeyHash::operator()(const FFbxVertexKey& Key) const noexcept
{
	size_t Seed = std::hash<int32>{}(Key.ControlPointIndex);
	Seed ^= std::hash<int32>{}(Key.PolygonIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
	Seed ^= std::hash<int32>{}(Key.CornerIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
	Seed ^= std::hash<int32>{}(Key.MaterialIndex) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
	return Seed;
}

bool FFbxCommon::ParseFbx(const FString& FbxFilePath, FFbxInfo& OutContext)
{
	OutContext.Manager = FbxManager::Create();
	if (!OutContext.Manager)
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to create FBX SDK manager.");
		return false;
	}

	FbxIOSettings* IOSettings = FbxIOSettings::Create(OutContext.Manager, IOSROOT);
	OutContext.Manager->SetIOSettings(IOSettings);

	FbxImporter* Importer = FbxImporter::Create(OutContext.Manager, "");
	if (!Importer->Initialize(FbxFilePath.c_str(), -1, OutContext.Manager->GetIOSettings()))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to initialize FBX importer: %s", FbxFilePath.c_str());
		Importer->Destroy();
		Destroy(OutContext);
		return false;
	}

	OutContext.Scene = FbxScene::Create(OutContext.Manager, "ImportedFbxScene");
	if (!Importer->Import(OutContext.Scene))
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Failed to import FBX scene: %s", FbxFilePath.c_str());
		Importer->Destroy();
		Destroy(OutContext);
		return false;
	}

	Importer->Destroy();

	const FbxAxisSystem TargetAxisSystem = FbxAxisSystem::MayaZUp;
	const FbxAxisSystem SourceAxisSystem = OutContext.Scene->GetGlobalSettings().GetAxisSystem();
	if (SourceAxisSystem != TargetAxisSystem)
	{
		TargetAxisSystem.ConvertScene(OutContext.Scene);
	}

	FbxGeometryConverter GeometryConverter(OutContext.Manager);
	GeometryConverter.Triangulate(OutContext.Scene, true);
	return true;
}

void FFbxCommon::Destroy(FFbxInfo& Context)
{
	if (Context.Manager)
	{
		Context.Manager->Destroy();
	}

	Context = FFbxInfo();
}

int32 FFbxCommon::GetPolygonMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex)
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

int32 FFbxCommon::FindOrAddMaterial(const FString& FbxFilePath, FbxNode* Node, int32 LocalMaterialIndex, FFbxInfo& Context)
{
	FbxSurfaceMaterial* SourceMaterial = Node ? Node->GetMaterial(LocalMaterialIndex) : nullptr;
	if (!SourceMaterial)
	{
		if (Context.Materials.empty())
		{
			FFbxMaterialInfo DefaultMaterial;
			Context.Materials.push_back(DefaultMaterial);
		}
		return 0;
	}

	auto It = Context.MaterialMap.find(SourceMaterial);
	if (It != Context.MaterialMap.end())
	{
		return It->second;
	}

	FFbxMaterialInfo MaterialInfo;
	MaterialInfo.SourceMaterial = SourceMaterial;
	MaterialInfo.MaterialSlotName = SanitizeName(SourceMaterial->GetName());
	MaterialInfo.DiffuseTexturePath = FindDiffuseTexturePath(FbxFilePath, SourceMaterial);
	MaterialInfo.DiffuseColor = GetDiffuseColor(SourceMaterial);

	const int32 MaterialIndex = static_cast<int32>(Context.Materials.size());
	Context.Materials.push_back(MaterialInfo);
	Context.MaterialMap.emplace(SourceMaterial, MaterialIndex);
	return MaterialIndex;
}

FString FFbxCommon::ConvertMaterialInfoToMaterialAsset(const FString& FbxFilePath, const FFbxMaterialInfo& MaterialInfo)
{
	if (!MaterialInfo.SourceMaterial)
	{
		return "None";
	}

	const FString SourceAssetName = SanitizeName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(FbxFilePath)).stem().wstring()));
	const FString SlotAssetName = SanitizeName(MaterialInfo.MaterialSlotName.empty() ? FString("None") : MaterialInfo.MaterialSlotName);

	std::filesystem::path MaterialDirectory = std::filesystem::path(FPaths::ContentDir()) / L"Materials";
	std::filesystem::path MaterialAssetPathW = MaterialDirectory / (L"M_" + FPaths::ToWide(SlotAssetName) + L".uasset");
	const FString MaterialAssetPath = MakeProjectRelativePath(MaterialAssetPathW);

	if (std::filesystem::exists(MaterialAssetPathW))
	{
		return MaterialAssetPath;
	}

	std::filesystem::create_directories(MaterialDirectory);

	json::JSON JsonData;
	JsonData["PathFileName"] = MaterialAssetPath;
	JsonData["Origin"] = "FbxImport";
	JsonData["SourceFilePath"] = MakeProjectRelativePath(std::filesystem::path(FPaths::ToWide(FbxFilePath)));
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

	FMaterialManager::Get().CreateMaterialAssetFromJson(MaterialAssetPath, JsonData);
	return MaterialAssetPath;
}

FString FFbxCommon::SanitizeName(FString Name)
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

FVector FFbxCommon::GetDiffuseColor(FbxSurfaceMaterial* FbxMaterial)
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

FString FFbxCommon::FindDiffuseTexturePath(const FString& FbxFilePath, FbxSurfaceMaterial* FbxMaterial)
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

FString FFbxCommon::ResolveFbxTexturePath(const FString& FbxFilePath, FbxFileTexture* Texture)
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

FString FFbxCommon::MakeProjectRelativePath(const std::filesystem::path& Path)
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

bool FFbxCommon::ReadNormal(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonIndex, int32 CornerIndex, FbxVector4& OutNormal)
{
	if (!Mesh)
	{
		return false;
	}

	const FbxGeometryElementNormal* NormalElement = Mesh->GetElementNormal(0);
	if (NormalElement)
	{
		int32 ElementIndex = -1;
		switch (NormalElement->GetMappingMode())
		{
		case FbxLayerElement::eByControlPoint:
			ElementIndex = ControlPointIndex;
			break;
		case FbxLayerElement::eByPolygonVertex:
			ElementIndex = GetPolygonVertexLinearIndex(Mesh, PolygonIndex, CornerIndex);
			break;
		case FbxLayerElement::eByPolygon:
			ElementIndex = PolygonIndex;
			break;
		case FbxLayerElement::eAllSame:
			ElementIndex = 0;
			break;
		default:
			ElementIndex = -1;
			break;
		}

		const int32 DirectIndex = ResolveLayerElementIndex(NormalElement, ElementIndex);
		if (DirectIndex >= 0 && DirectIndex < NormalElement->GetDirectArray().GetCount())
		{
			OutNormal = NormalElement->GetDirectArray().GetAt(DirectIndex);
			return true;
		}
	}

	return Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, OutNormal);
}

FVector FFbxCommon::RemapVector(const FbxVector4& Vector)
{
	return FVector(
		static_cast<float>(-Vector[1]),
		static_cast<float>(-Vector[0]),
		static_cast<float>(Vector[2]));
}

FbxVector4 FFbxCommon::UnmapVector(const FVector& Vector)
{
	return FbxVector4(
		static_cast<double>(-Vector.Y),
		static_cast<double>(-Vector.X),
		static_cast<double>(Vector.Z),
		0.0);
}

FMatrix FFbxCommon::MakeEngineLinearMatrix(const FbxAMatrix& Matrix)
{
	const FVector EngineBasisX = RemapVector(Matrix.MultR(UnmapVector(FVector(1.0f, 0.0f, 0.0f))));
	const FVector EngineBasisY = RemapVector(Matrix.MultR(UnmapVector(FVector(0.0f, 1.0f, 0.0f))));
	const FVector EngineBasisZ = RemapVector(Matrix.MultR(UnmapVector(FVector(0.0f, 0.0f, 1.0f))));

	FMatrix Result = FMatrix::Identity;
	Result.M[0][0] = EngineBasisX.X; Result.M[0][1] = EngineBasisX.Y; Result.M[0][2] = EngineBasisX.Z; Result.M[0][3] = 0.0f;
	Result.M[1][0] = EngineBasisY.X; Result.M[1][1] = EngineBasisY.Y; Result.M[1][2] = EngineBasisY.Z; Result.M[1][3] = 0.0f;
	Result.M[2][0] = EngineBasisZ.X; Result.M[2][1] = EngineBasisZ.Y; Result.M[2][2] = EngineBasisZ.Z; Result.M[2][3] = 0.0f;
	Result.M[3][0] = 0.0f; Result.M[3][1] = 0.0f; Result.M[3][2] = 0.0f; Result.M[3][3] = 1.0f;
	return Result;
}

FVector FFbxCommon::TransformNormalByMatrix(const FbxAMatrix& Matrix, const FbxVector4& Normal)
{
	const FVector EngineNormal = RemapVector(Normal);
	const FMatrix EngineLinear = MakeEngineLinearMatrix(Matrix);

	FVector Result;
	if (std::fabs(EngineLinear.GetBasisDeterminant3x3()) > 1.0e-8f)
	{
		Result = EngineLinear.GetInverse().GetTransposed().TransformVector(EngineNormal);
	}
	else
	{
		Result = EngineLinear.TransformVector(EngineNormal);
	}

	Result = Result.Normalized();
	if (Result.IsNearlyZero() || !IsFiniteVector(Result))
	{
		return FVector(0.0f, 0.0f, 1.0f);
	}
	return Result;
}

FVector2 FFbxCommon::RemapUV(const FbxVector2& UV)
{
	return FVector2(
		static_cast<float>(UV[0]),
		1.0f - static_cast<float>(UV[1]));
}
#endif
