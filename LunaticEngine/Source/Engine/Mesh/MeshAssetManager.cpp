#include "PCH/LunaticPCH.h"
#include "Mesh/MeshAssetManager.h"

#include "Engine/Asset/AssetFileSerializer.h"
#include "Core/Log.h"
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
#include <cwctype>
#include <vector>

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
            return FPaths::NormalizePath(FPaths::ToUtf8(Relative.generic_wstring()));
        }
        return FPaths::NormalizePath(FPaths::ToUtf8(Normalized.generic_wstring()));
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

    std::filesystem::path ResolvePathAgainstProjectRoot(const FString& Path)
    {
        return std::filesystem::path(FPaths::ResolvePathToDisk(Path)).lexically_normal();
    }

    bool IsUnderDirectory(const std::filesystem::path& Parent, const std::filesystem::path& Child)
    {
        const std::filesystem::path Rel = Child.lexically_normal().lexically_relative(Parent.lexically_normal());
        return !Rel.empty() && Rel.native().find(L"..") != 0;
    }

    std::wstring ToTitleStem(std::wstring Stem)
    {
        bool bCapitalizeNext = true;
        for (wchar_t& Character : Stem)
        {
            if (bCapitalizeNext && Character != L'_' && Character != L'-')
            {
                Character = static_cast<wchar_t>(std::towupper(Character));
                bCapitalizeNext = false;
            }
            else
            {
                bCapitalizeNext = (Character == L'_' || Character == L'-');
            }
        }
        return Stem;
    }

    FString MakeMeshAssetPath(const FString& SourceOrAssetPath, const wchar_t* Prefix)
    {
        if (IsUAssetPath(SourceOrAssetPath))
        {
            return SourceOrAssetPath;
        }

        const std::filesystem::path SourcePath = ResolvePathAgainstProjectRoot(SourceOrAssetPath);
        const std::filesystem::path EngineBasicShapeRoot = std::filesystem::path(FPaths::EngineBasicShapeSourceDir()).lexically_normal();
        const std::wstring Stem = ToTitleStem(SourcePath.stem().wstring());

        if (IsUnderDirectory(EngineBasicShapeRoot, SourcePath))
        {
            const std::filesystem::path RelativeSource = SourcePath.lexically_relative(EngineBasicShapeRoot);
            const std::filesystem::path RelativeParent = RelativeSource.parent_path();
            std::filesystem::path AssetPath = std::filesystem::path(FPaths::BasicShapeDir()) / RelativeParent / AddPrefixIfNeeded(Stem, Prefix);
            AssetPath += L".uasset";
            return FPaths::NormalizePath(FPaths::ToUtf8(AssetPath.lexically_normal().generic_wstring()));
        }

        std::filesystem::path AssetPath = std::filesystem::path(FPaths::ContentDir()) / L"Meshes" / Stem / AddPrefixIfNeeded(Stem, Prefix);
        AssetPath += L".uasset";
        return FPaths::NormalizePath(FPaths::ToUtf8(AssetPath.lexically_normal().generic_wstring()));
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


    FString NormalizeMeshAssetCacheKey(const FString& Path)
    {
        if (Path.empty())
        {
            return {};
        }

        return FPaths::NormalizePath(Path);
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
    const std::filesystem::path ScanRoots[] = {
        std::filesystem::path(FPaths::ContentDir()),
        std::filesystem::path(FPaths::EngineContentDir())
    };

    std::vector<std::filesystem::path> UniqueRoots;
    for (const std::filesystem::path& ContentRoot : ScanRoots)
    {
        const std::filesystem::path NormalizedRoot = ContentRoot.lexically_normal();
        if (std::find(UniqueRoots.begin(), UniqueRoots.end(), NormalizedRoot) != UniqueRoots.end())
        {
            continue;
        }
        UniqueRoots.push_back(NormalizedRoot);

        std::error_code ErrorCode;
        if (!std::filesystem::exists(NormalizedRoot, ErrorCode) || !std::filesystem::is_directory(NormalizedRoot, ErrorCode))
        {
            continue;
        }

        std::filesystem::recursive_directory_iterator It(
            NormalizedRoot,
            std::filesystem::directory_options::skip_permission_denied,
            ErrorCode);
        const std::filesystem::recursive_directory_iterator End;
        for (; It != End; It.increment(ErrorCode))
        {
            if (ErrorCode)
            {
                ErrorCode.clear();
                continue;
            }

            const std::filesystem::directory_entry& Entry = *It;
            if (!Entry.is_regular_file(ErrorCode))
            {
                ErrorCode.clear();
                continue;
            }

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
}

void FMeshAssetManager::ScanMeshSourceFiles()
{
    AvailableMeshSourceFiles.clear();

    const std::filesystem::path ScanRoots[] = {
        std::filesystem::path(FPaths::EngineSourceDir())
    };

    for (const std::filesystem::path& DataRoot : ScanRoots)
    {
        std::error_code ErrorCode;
        if (!std::filesystem::exists(DataRoot, ErrorCode) || !std::filesystem::is_directory(DataRoot, ErrorCode))
        {
            continue;
        }

        std::filesystem::recursive_directory_iterator It(
            DataRoot,
            std::filesystem::directory_options::skip_permission_denied,
            ErrorCode);
        const std::filesystem::recursive_directory_iterator End;
        for (; It != End; It.increment(ErrorCode))
        {
            if (ErrorCode)
            {
                ErrorCode.clear();
                continue;
            }

            const std::filesystem::directory_entry& Entry = *It;
            if (!Entry.is_regular_file(ErrorCode))
            {
                ErrorCode.clear();
                continue;
            }

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
}

const TArray<FMeshAssetListItem>& FMeshAssetManager::GetAvailableMeshFiles()
{
    return AvailableMeshFiles;
}

const TArray<FMeshAssetListItem>& FMeshAssetManager::GetAvailableMeshSourceFiles()
{
    return AvailableMeshSourceFiles;
}

UStaticMesh* FMeshAssetManager::LoadStaticMeshAssetFile(const FString& AssetPath, ID3D11Device* InDevice, EMeshAssetLoadPurpose Purpose)
{
    const FString CacheKey = NormalizeMeshAssetCacheKey(AssetPath);
    if (Purpose == EMeshAssetLoadPurpose::RuntimeShared)
    {
        auto It = StaticMeshCache.find(CacheKey);
        if (It != StaticMeshCache.end())
        {
            return It->second;
        }
    }

    UE_LOG_CATEGORY(MeshAssetManager, Info, "[AssetLoad] StaticMesh .uasset load begin: path=%s", CacheKey.c_str());
    FString Error;
    UObject* LoadedObject = FAssetFileSerializer::LoadObjectFromAssetFile(AssetPath, &Error);
    UStaticMesh* StaticMesh = Cast<UStaticMesh>(LoadedObject);
    if (!StaticMesh)
    {
        if (LoadedObject)
        {
            UObjectManager::Get().DestroyObject(LoadedObject);
        }
        UE_LOG_CATEGORY(MeshAssetManager, Error, "[AssetLoad] StaticMesh .uasset load failed: path=%s error=%s", CacheKey.c_str(), Error.c_str());
        return nullptr;
    }

    UE_LOG_CATEGORY(MeshAssetManager, Info, "[AssetLoad] StaticMesh .uasset loaded: path=%s", CacheKey.c_str());

    if (FStaticMesh *MeshAsset = StaticMesh->GetStaticMeshAsset())
    {
        // Component references should serialize the imported .uasset path, not the original source file path.
        MeshAsset->PathFileName = CacheKey;
    }

    StaticMesh->InitResources(InDevice);
    if (Purpose == EMeshAssetLoadPurpose::RuntimeShared)
    {
        StaticMeshCache[CacheKey] = StaticMesh;
    }
    return StaticMesh;
}

USkeletalMesh* FMeshAssetManager::LoadSkeletalMeshAssetFile(const FString& AssetPath, ID3D11Device* InDevice, EMeshAssetLoadPurpose Purpose)
{
    (void)InDevice;

    const FString CacheKey = NormalizeMeshAssetCacheKey(AssetPath);
    if (Purpose == EMeshAssetLoadPurpose::RuntimeShared)
    {
        auto It = SkeletalMeshCache.find(CacheKey);
        if (It != SkeletalMeshCache.end())
        {
            return It->second;
        }
    }

    UE_LOG_CATEGORY(MeshAssetManager, Info, "[AssetLoad] SkeletalMesh .uasset load begin: path=%s", CacheKey.c_str());
    FString Error;
    UObject* LoadedObject = FAssetFileSerializer::LoadObjectFromAssetFile(AssetPath, &Error);
    USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadedObject);
    if (!SkeletalMesh)
    {
        if (LoadedObject)
        {
            UObjectManager::Get().DestroyObject(LoadedObject);
        }
        UE_LOG_CATEGORY(MeshAssetManager, Error, "[AssetLoad] SkeletalMesh .uasset load failed: path=%s error=%s", CacheKey.c_str(), Error.c_str());
        return nullptr;
    }

    UE_LOG_CATEGORY(MeshAssetManager, Info, "[AssetLoad] SkeletalMesh .uasset loaded: path=%s", CacheKey.c_str());

    if (FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset())
    {
        // Component references should serialize the imported .uasset path, not the original source file path.
        MeshAsset->PathFileName = CacheKey;
        MeshAsset->BuildBoneHierarchyCache();
    }

    if (Purpose == EMeshAssetLoadPurpose::RuntimeShared)
    {
        SkeletalMeshCache[CacheKey] = SkeletalMesh;
    }
    return SkeletalMesh;
}

void FMeshAssetManager::MarkAssetCacheStale(const FString& AssetPath)
{
    const FString CacheKey = NormalizeMeshAssetCacheKey(AssetPath);
    StaticMeshCache.erase(CacheKey);
    SkeletalMeshCache.erase(CacheKey);
}

UStaticMesh* FMeshAssetManager::LoadStaticMesh(const FString& PathFileName, const FImportOptions& Options, ID3D11Device* InDevice)
{
    if (IsUAssetPath(PathFileName))
    {
        return LoadStaticMeshAssetFile(PathFileName, InDevice);
    }

    const FString AssetPath = GetStaticMeshAssetPath(PathFileName);
    StaticMeshCache.erase(NormalizeMeshAssetCacheKey(AssetPath));

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

    NewMeshAsset->PathFileName = AssetPath;
    StaticMesh->SetFName(FName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(AssetPath)).stem().wstring())));
    StaticMesh->SetStaticMaterials(std::move(ParsedMaterials));
    StaticMesh->SetStaticMeshAsset(NewMeshAsset);

    EnsureParentDirectoryExists(AssetPath);
    FString Error;
    if (!FAssetFileSerializer::SaveObjectToAssetFile(AssetPath, StaticMesh, &Error))
    {
        UE_LOG_CATEGORY(MeshAssetManager, Error, "[AssetSave] StaticMesh .uasset save failed: source=%s target=%s error=%s", PathFileName.c_str(), NormalizeMeshAssetCacheKey(AssetPath).c_str(), Error.c_str());
    }
    else
    {
        UE_LOG_CATEGORY(MeshAssetManager, Info, "[AssetSave] StaticMesh .uasset saved: source=%s target=%s", PathFileName.c_str(), NormalizeMeshAssetCacheKey(AssetPath).c_str());
    }

    StaticMesh->InitResources(InDevice);
    StaticMeshCache[NormalizeMeshAssetCacheKey(AssetPath)] = StaticMesh;

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

    NewMeshAsset->PathFileName = AssetPath;
    SkeletalMesh->SetFName(FName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(AssetPath)).stem().wstring())));
    SkeletalMesh->SetStaticMaterials(std::move(ParsedMaterials));
    SkeletalMesh->SetSkeletalMeshAsset(NewMeshAsset);

    EnsureParentDirectoryExists(AssetPath);
    FString Error;
    if (!FAssetFileSerializer::SaveObjectToAssetFile(AssetPath, SkeletalMesh, &Error))
    {
        UE_LOG_CATEGORY(MeshAssetManager, Error, "[AssetSave] SkeletalMesh .uasset save failed: source=%s target=%s error=%s", PathFileName.c_str(), NormalizeMeshAssetCacheKey(AssetPath).c_str(), Error.c_str());
    }
    else
    {
        UE_LOG_CATEGORY(MeshAssetManager, Info, "[AssetSave] SkeletalMesh .uasset saved: source=%s target=%s", PathFileName.c_str(), NormalizeMeshAssetCacheKey(AssetPath).c_str());
    }

    SkeletalMeshCache[NormalizeMeshAssetCacheKey(AssetPath)] = SkeletalMesh;

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
