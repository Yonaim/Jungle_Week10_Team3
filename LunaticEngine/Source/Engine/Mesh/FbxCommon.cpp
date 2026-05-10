#include "Mesh/FbxCommon.h"

#include "Core/Log.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"
#include "Engine/Core/SimpleJsonWrapper.h"

#include <fstream>

#if defined(_WIN64)
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

FString FFbxCommon::ConvertMaterialInfoToMat(const FFbxMaterialInfo& MaterialInfo)
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

FVector FFbxCommon::RemapVector(const FbxVector4& Vector)
{
	return FVector(
		static_cast<float>(-Vector[1]),
		static_cast<float>(-Vector[0]),
		static_cast<float>(Vector[2]));
}

FVector2 FFbxCommon::RemapUV(const FbxVector2& UV)
{
	return FVector2(
		static_cast<float>(UV[0]),
		1.0f - static_cast<float>(UV[1]));
}
#endif
