#include "PCH/LunaticPCH.h"
#include "Mesh/MeshAssetManager.h"

#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Mesh/FbxCommon.h"
#include "Mesh/FbxImporter.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/StaticMesh.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <filesystem>

TMap<FString, UStaticMesh*> FMeshAssetManager::StaticMeshCache;
TMap<FString, USkeletalMesh*> FMeshAssetManager::SkeletalMeshCache;
TArray<FMeshAssetListItem> FMeshAssetManager::AvailableMeshFiles;
TArray<FMeshAssetListItem> FMeshAssetManager::AvailableMeshSourceFiles;

namespace
{
    std::wstring GetLowerExtensionWide(const FString& Path)
    {
        std::filesystem::path PathW(FPaths::ToWide(Path));
        std::wstring Ext = PathW.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        return Ext;
    }

    bool IsUAssetPath(const FString& Path)
    {
        return GetLowerExtensionWide(Path) == L".uasset";
    }

    FString ToProjectRelativePath(const std::filesystem::path& Path)
    {
        const std::filesystem::path ProjectRoot(FPaths::RootDir());
        std::filesystem::path Normalized = Path.lexically_normal();
        std::filesystem::path Relative = Normalized.lexically_relative(ProjectRoot);
        if (!Relative.empty() && Relative.native().find(L"..") != 0)
        {
            return FPaths::ToUtf8(Relative.generic_wstring());
        }
        return FPaths::ToUtf8(Normalized.generic_wstring());
    }

    std::wstring AddPrefixIfNeeded(const std::wstring& Stem, const wchar_t* Prefix)
    {
        const std::wstring PrefixString(Prefix);
        if (Stem.rfind(PrefixString, 0) == 0)
        {
            return Stem;
        }
        return PrefixString + Stem;
    }

    FString MakeMeshAssetPath(const FString& SourceOrAssetPath, const wchar_t* Prefix)
    {
        if (IsUAssetPath(SourceOrAssetPath))
        {
            return SourceOrAssetPath;
        }

        std::filesystem::path SourcePath(FPaths::ToWide(SourceOrAssetPath));
        std::filesystem::path AssetPath = SourcePath.parent_path() / AddPrefixIfNeeded(SourcePath.stem().wstring(), Prefix);
        AssetPath += L".uasset";
        return FPaths::ToUtf8(AssetPath.lexically_normal().generic_wstring());
    }

    bool IsSourceNewerThanAsset(const FString& SourcePath, const FString& AssetPath)
    {
        const std::filesystem::path SourcePathW(FPaths::ToWide(SourcePath));
        const std::filesystem::path AssetPathW(FPaths::ToWide(AssetPath));
        if (!std::filesystem::exists(AssetPathW))
        {
            return true;
        }
        if (!std::filesystem::exists(SourcePathW))
        {
            return false;
        }
        return std::filesystem::last_write_time(SourcePathW) > std::filesystem::last_write_time(AssetPathW);
    }

    void EnsureParentDirectoryExists(const FString& Path)
    {
        const std::filesystem::path Parent = std::filesystem::path(FPaths::ToWide(Path)).parent_path();
        if (!Parent.empty())
        {
            FPaths::CreateDir(Parent.wstring());
        }
    }

    bool HasSkinDeformer(FbxNode* Node)
    {
        if (!Node)
        {
            return false;
        }

        if (FbxMesh* Mesh = Node->GetMesh())
        {
            if (Mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
            {
                return true;
            }
        }

        const int32 ChildCount = Node->GetChildCount();
        for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
        {
            if (HasSkinDeformer(Node->GetChild(ChildIndex)))
            {
                return true;
            }
        }
        return false;
    }
}

FString FMeshAssetManager::GetStaticMeshAssetPath(const FString& SourceOrAssetPath)
{
    return MakeMeshAssetPath(SourceOrAssetPath, L"SM_");
}

FString FMeshAssetManager::GetSkeletalMeshAssetPath(const FString& SourceOrAssetPath)
{
    return MakeMeshAssetPath(SourceOrAssetPath, L"SK_");
}

bool FMeshAssetManager::IsFbxSkeletalMesh(const FString& FbxPath)
{
    if (GetLowerExtensionWide(FbxPath) != L".fbx")
    {
        return false;
    }

    FFbxInfo Context;
    const bool bParsed = FFbxCommon::ParseFbx(FbxPath, Context);
    const bool bHasSkin = bParsed && HasSkinDeformer(Context.Scene ? Context.Scene->GetRootNode() : nullptr);
    FFbxCommon::Destroy(Context);
    return bHasSkin;
}

void FMeshAssetManager::ScanMeshAssets()
{
    AvailableMeshFiles.clear();

    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    const std::filesystem::path ContentRoot = FPaths::ContentDir();
    if (!std::filesystem::exists(ContentRoot))
    {
        return;
    }

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(ContentRoot))
    {
        if (!Entry.is_regular_file()) continue;

        const std::filesystem::path& Path = Entry.path();
        if (Path.extension() != L".uasset") continue;

        FAssetFileHeader Header;
        if (!FAssetFileSerializer::ReadAssetHeader(Path, Header)) continue;
        if (Header.ClassId != EAssetClassId::StaticMesh && Header.ClassId != EAssetClassId::SkeletalMesh) continue;

        FMeshAssetListItem Item;
        Item.DisplayName = Header.AssetName.empty() ? FPaths::ToUtf8(Path.stem().wstring()) : Header.AssetName;
        Item.FullPath = ToProjectRelativePath(Path);
        Item.AssetClassId = Header.ClassId;
        AvailableMeshFiles.push_back(std::move(Item));
    }
}

void FMeshAssetManager::ScanMeshSourceFiles()
{
    AvailableMeshSourceFiles.clear();

    const std::filesystem::path DataRoot = FPaths::ContentDir();
    if (!std::filesystem::exists(DataRoot))
    {
        return;
    }

    for (const auto& Entry : std::filesystem::recursive_directory_iterator(DataRoot))
    {
        if (!Entry.is_regular_file()) continue;

        const std::filesystem::path& Path = Entry.path();
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        if (Ext != L".obj" && Ext != L".fbx") continue;

        FMeshAssetListItem Item;
        Item.DisplayName = FPaths::ToUtf8(Path.filename().wstring());
        Item.FullPath = ToProjectRelativePath(Path);
        Item.AssetClassId = EAssetClassId::Unknown;
        AvailableMeshSourceFiles.push_back(std::move(Item));
    }
}

const TArray<FMeshAssetListItem>& FMeshAssetManager::GetAvailableMeshFiles()
{
    return AvailableMeshFiles;
}

const TArray<FMeshAssetListItem>& FMeshAssetManager::GetAvailableMeshSourceFiles()
{
    return AvailableMeshSourceFiles;
}

UStaticMesh* FMeshAssetManager::LoadStaticMeshAssetFile(const FString& AssetPath, ID3D11Device* InDevice)
{
    auto It = StaticMeshCache.find(AssetPath);
    if (It != StaticMeshCache.end())
    {
        return It->second;
    }

    FString Error;
    UObject* LoadedObject = FAssetFileSerializer::LoadObjectFromAssetFile(AssetPath, &Error);
    UStaticMesh* StaticMesh = Cast<UStaticMesh>(LoadedObject);
    if (!StaticMesh)
    {
        if (LoadedObject)
        {
            UObjectManager::Get().DestroyObject(LoadedObject);
        }
        return nullptr;
    }

    StaticMesh->InitResources(InDevice);
    StaticMeshCache[AssetPath] = StaticMesh;
    return StaticMesh;
}

USkeletalMesh* FMeshAssetManager::LoadSkeletalMeshAssetFile(const FString& AssetPath, ID3D11Device* InDevice)
{
    (void)InDevice;

    auto It = SkeletalMeshCache.find(AssetPath);
    if (It != SkeletalMeshCache.end())
    {
        return It->second;
    }

    FString Error;
    UObject* LoadedObject = FAssetFileSerializer::LoadObjectFromAssetFile(AssetPath, &Error);
    USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadedObject);
    if (!SkeletalMesh)
    {
        if (LoadedObject)
        {
            UObjectManager::Get().DestroyObject(LoadedObject);
        }
        return nullptr;
    }

    if (FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset())
    {
        MeshAsset->BuildBoneHierarchyCache();
    }

    SkeletalMeshCache[AssetPath] = SkeletalMesh;
    return SkeletalMesh;
}

UStaticMesh* FMeshAssetManager::LoadStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
    if (IsUAssetPath(PathFileName))
    {
        return LoadStaticMeshAssetFile(PathFileName, InDevice);
    }

    const FString AssetPath = GetStaticMeshAssetPath(PathFileName);
    StaticMeshCache.erase(AssetPath);

    UStaticMesh* StaticMesh = UObjectManager::Get().CreateObject<UStaticMesh>();
    FStaticMesh* NewMeshAsset = new FStaticMesh();
    TArray<FStaticMaterial> ParsedMaterials;

    const bool bImported = GetLowerExtensionWide(PathFileName) == L".fbx"
        ? FFbxStaticMeshImporter::Import(PathFileName, *NewMeshAsset, ParsedMaterials)
        : FObjImporter::Import(PathFileName, Options, *NewMeshAsset, ParsedMaterials);

    if (!bImported)
    {
        delete NewMeshAsset;
        UObjectManager::Get().DestroyObject(StaticMesh);
        return nullptr;
    }

    NewMeshAsset->PathFileName = PathFileName;
    StaticMesh->SetFName(FName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(AssetPath)).stem().wstring())));
    StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
    StaticMesh->SetStaticMeshAsset(NewMeshAsset);

    EnsureParentDirectoryExists(AssetPath);
    FString Error;
    FAssetFileSerializer::SaveObjectToAssetFile(AssetPath, StaticMesh, &Error);

    StaticMesh->InitResources(InDevice);
    StaticMeshCache[AssetPath] = StaticMesh;

    ScanMeshAssets();
    FMaterialManager::Get().ScanMaterialAssets();
    return StaticMesh;
}

UStaticMesh* FMeshAssetManager::LoadStaticMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
    const std::wstring RequestedExt = GetLowerExtensionWide(PathFileName);
    if (RequestedExt == L".uasset")
    {
        return LoadStaticMeshAssetFile(PathFileName, InDevice);
    }

    if (RequestedExt == L".fbx" && IsFbxSkeletalMesh(PathFileName))
    {
        LoadSkeletalMesh(PathFileName, InDevice);
        return nullptr;
    }

    const FString AssetPath = GetStaticMeshAssetPath(PathFileName);
    if (!IsSourceNewerThanAsset(PathFileName, AssetPath))
    {
        if (UStaticMesh* Loaded = LoadStaticMeshAssetFile(AssetPath, InDevice))
        {
            return Loaded;
        }
    }

    FImportOptions DefaultOptions;
    return LoadStaticMesh(PathFileName, DefaultOptions, InDevice);
}

USkeletalMesh* FMeshAssetManager::LoadSkeletalMesh(const FString& PathFileName, ID3D11Device* InDevice)
{
    const std::wstring RequestedExt = GetLowerExtensionWide(PathFileName);
    if (RequestedExt == L".uasset")
    {
        return LoadSkeletalMeshAssetFile(PathFileName, InDevice);
    }

    if (RequestedExt != L".fbx" || !IsFbxSkeletalMesh(PathFileName))
    {
        LoadStaticMesh(PathFileName, InDevice);
        return nullptr;
    }

    const FString AssetPath = GetSkeletalMeshAssetPath(PathFileName);
    if (!IsSourceNewerThanAsset(PathFileName, AssetPath))
    {
        if (USkeletalMesh* Loaded = LoadSkeletalMeshAssetFile(AssetPath, InDevice))
        {
            return Loaded;
        }
    }

    USkeletalMesh* SkeletalMesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
    FSkeletalMesh* NewMeshAsset = new FSkeletalMesh();
    TArray<FStaticMaterial> ParsedMaterials;

    if (!FFbxSkeletalMeshImporter::Import(PathFileName, *NewMeshAsset, ParsedMaterials))
    {
        delete NewMeshAsset;
        UObjectManager::Get().DestroyObject(SkeletalMesh);
        return nullptr;
    }

    SkeletalMesh->SetFName(FName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(AssetPath)).stem().wstring())));
    SkeletalMesh->SetStaticMaterials(std::move(ParsedMaterials));
    SkeletalMesh->SetSkeletalMeshAsset(NewMeshAsset);

    EnsureParentDirectoryExists(AssetPath);
    FString Error;
    FAssetFileSerializer::SaveObjectToAssetFile(AssetPath, SkeletalMesh, &Error);

    SkeletalMeshCache[AssetPath] = SkeletalMesh;

    ScanMeshAssets();
    FMaterialManager::Get().ScanMaterialAssets();
    return SkeletalMesh;
}

void FMeshAssetManager::ReleaseAllGPU()
{
    for (auto& [Key, Mesh] : StaticMeshCache)
    {
        if (!Mesh)
        {
            continue;
        }

        FStaticMesh* Asset = Mesh->GetStaticMeshAsset();
        if (Asset && Asset->RenderBuffer)
        {
            Asset->RenderBuffer->Release();
            Asset->RenderBuffer.reset();
        }

        for (uint32 LOD = 1; LOD < UStaticMesh::MAX_LOD_COUNT; ++LOD)
        {
            FMeshBuffer* LODBuffer = Mesh->GetLODMeshBuffer(LOD);
            if (LODBuffer)
            {
                LODBuffer->Release();
            }
        }
    }

    StaticMeshCache.clear();
    SkeletalMeshCache.clear();
}
