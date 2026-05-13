#include "PCH/LunaticPCH.h"
#include "AssetTools/AssetImportManager.h"

#include "Common/File/EditorFileUtils.h"
#include "Common/UI/Notifications/NotificationToast.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "EditorEngine.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Core/SimpleJsonWrapper.h"
#include "Engine/Platform/Paths.h"
#include "Materials/MaterialManager.h"
#include "Mesh/FbxImporter.h"
#include "Mesh/MeshAssetManager.h"
#include "Mesh/ObjImporter.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshCommon.h"
#include "Mesh/StaticMesh.h"
#include "Mesh/StaticMeshCommon.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Texture/Texture2D.h"

#include <algorithm>
#include <filesystem>
#include <cwctype>

namespace
{
    std::wstring ToLowerExtension(const std::filesystem::path& Path)
    {
        std::wstring Ext = Path.extension().wstring();
        std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
        return Ext;
    }

    std::filesystem::path ResolveSourcePathOnDisk(const FString& SourcePath)
    {
        std::filesystem::path Path(FPaths::ToWide(SourcePath));
        if (Path.is_absolute())
        {
            return Path.lexically_normal();
        }

        std::filesystem::path RootRelative = std::filesystem::path(FPaths::RootDir()) / Path;
        if (std::filesystem::exists(RootRelative))
        {
            return RootRelative.lexically_normal();
        }

        return Path.lexically_normal();
    }

    FString ToUtf8GenericPath(const std::filesystem::path& Path)
    {
        return FPaths::ToUtf8(Path.lexically_normal().generic_wstring());
    }

    FString SanitizeAssetName(FString Name)
    {
        if (Name.empty())
        {
            return "Imported";
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

    void EnsureUniquePath(std::filesystem::path& InOutPath)
    {
        if (!std::filesystem::exists(InOutPath))
        {
            return;
        }

        const std::filesystem::path Parent = InOutPath.parent_path();
        const std::wstring Stem = InOutPath.stem().wstring();
        const std::wstring Extension = InOutPath.extension().wstring();

        int32 Suffix = 1;
        do
        {
            InOutPath = Parent / (Stem + L"_" + std::to_wstring(Suffix++) + Extension);
        } while (std::filesystem::exists(InOutPath));
    }

    const char* GetAssetClassIdName(EAssetClassId ClassId)
    {
        switch (ClassId)
        {
        case EAssetClassId::StaticMesh:   return "UStaticMesh";
        case EAssetClassId::SkeletalMesh: return "USkeletalMesh";
        case EAssetClassId::Material:     return "UMaterial";
        case EAssetClassId::Texture:      return "UTexture2D";
        case EAssetClassId::PoseAsset:    return "USkeletonPoseAsset";
        default:                          return "UObject";
        }
    }
}

void FAssetImportManager::Init(UEditorEngine* InEditorEngine)
{
    EditorEngine = InEditorEngine;
}

void FAssetImportManager::Shutdown()
{
    EditorEngine = nullptr;
}

bool FAssetImportManager::ImportAssetWithDialog()
{
    if (!EditorEngine)
    {
        return false;
    }

    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"Importable Source Assets (*.fbx;*.obj;*.mtl;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.uasset)\0*.fbx;*.obj;*.mtl;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.uasset\0All Files (*.*)\0*.*\0",
        .Title = L"Import Source Asset",
        .InitialDirectory = FPaths::AssetDir().c_str(),
        .OwnerWindowHandle = EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr,
        .bFileMustExist = true,
        .bPathMustExist = true,
        .bPromptOverwrite = false,
        .bReturnRelativeToProjectRoot = false,
    });

    if (SelectedPath.empty())
    {
        UE_LOG_CATEGORY(AssetImport, Debug, "[Import] Import dialog canceled.");
        return false;
    }

    FString ImportedAssetPath;
    return ImportAssetFromPath(SelectedPath, &ImportedAssetPath);
}

bool FAssetImportManager::ImportAssetFromPath(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
    if (SourcePath.empty() || !std::filesystem::exists(SourceAbsolute))
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Source file not found: %s", SourcePath.c_str());
        FNotificationManager::Get().AddNotification("Import failed: source file not found.", ENotificationType::Error, 5.0f);
        return false;
    }

    const FString SourceFileNameForProgress = FPaths::ToUtf8(SourceAbsolute.filename().wstring());
    FNotificationToast::BeginProgress("Import to UAsset", "0%: preparing " + SourceFileNameForProgress, 0.0f);
    FNotificationManager::Get().AddNotification("Import started: " + SourceFileNameForProgress, ENotificationType::Info, 2.5f);
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Begin: source=%s", ToUtf8GenericPath(SourceAbsolute).c_str());

    const std::wstring Ext = ToLowerExtension(SourceAbsolute);
    bool bResult = false;

    if (Ext == L".fbx" || Ext == L".obj")
    {
        FNotificationToast::UpdateProgress("25%: converting mesh source", 0.25f);
        FNotificationManager::Get().AddNotification("Import 25%: converting mesh source", ENotificationType::Info, 2.5f);
        bResult = ImportMeshSource(SourcePath, OutImportedAssetPath);
    }
    else if (Ext == L".mtl")
    {
        FNotificationToast::UpdateProgress("25%: converting material source", 0.25f);
        FNotificationManager::Get().AddNotification("Import 25%: converting material source", ENotificationType::Info, 2.5f);
        bResult = ImportMtlSource(SourcePath, OutImportedAssetPath);
    }
    else if (Ext == L".uasset")
    {
        FNotificationToast::UpdateProgress("25%: copying uasset", 0.25f);
        FNotificationManager::Get().AddNotification("Import 25%: copying uasset", ENotificationType::Info, 2.5f);
        bResult = ImportExistingUAsset(SourcePath, OutImportedAssetPath);
    }
    else if (UTexture2D::IsSupportedTextureExtension(SourceAbsolute))
    {
        FNotificationToast::UpdateProgress("25%: converting texture source", 0.25f);
        FNotificationManager::Get().AddNotification("Import 25%: converting texture source", ENotificationType::Info, 2.5f);
        bResult = ImportTextureSource(SourcePath, OutImportedAssetPath);
    }
    else
    {
        UE_LOG_CATEGORY(AssetImport, Warning, "[Import] Unsupported source type: %s", ToUtf8GenericPath(SourceAbsolute).c_str());
        FNotificationToast::FinishProgress("Import failed: unsupported source file type", false, 5.0f);
        FNotificationManager::Get().AddNotification("Import failed: unsupported source file type.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (!bResult)
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Failed: source=%s", ToUtf8GenericPath(SourceAbsolute).c_str());
        FNotificationToast::FinishProgress("Import failed: see log for details", false, 5.0f);
        return false;
    }

    FNotificationToast::UpdateProgress("80%: refreshing asset registry", 0.80f);
    FNotificationManager::Get().AddNotification("Import 80%: refreshing asset registry", ENotificationType::Info, 2.0f);
    FMeshAssetManager::ScanMeshAssets();
    FMeshAssetManager::ScanMeshSourceFiles();
    FMaterialManager::Get().ScanMaterialAssets();
    UTexture2D::ScanTextureAssets();
    if (EditorEngine)
    {
        EditorEngine->RefreshContentBrowser();
    }

    const FString ImportedPath = OutImportedAssetPath ? *OutImportedAssetPath : FString();
    const FString SourceFileName = FPaths::ToUtf8(SourceAbsolute.filename().wstring());
    const FString ImportedFileName = !ImportedPath.empty()
        ? FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(ImportedPath)).filename().wstring())
        : FString("asset");
    const FString Message = !ImportedPath.empty()
        ? "Imported " + SourceFileName + " as " + ImportedFileName
        : "Imported " + SourceFileName;
    FNotificationToast::FinishProgress("100%: " + Message, true, 4.5f);
    FNotificationManager::Get().AddNotification(Message, ENotificationType::Success, 3.0f);
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] %s | source=%s target=%s", Message.c_str(), ToUtf8GenericPath(SourceAbsolute).c_str(), ImportedPath.c_str());

    if (EditorEngine && !ImportedPath.empty())
    {
        // Import는 .uasset 생성 및 Content Browser 선택까지만 수행한다.
        // Asset Editor는 사용자가 생성된 .uasset을 직접 열 때만 열린다.
        EditorEngine->SelectContentBrowserPath(ImportedPath);
    }

    return true;
}

bool FAssetImportManager::ImportMaterialWithDialog()
{
    return ImportAssetWithDialog();
}

bool FAssetImportManager::ImportTextureWithDialog()
{
    return ImportAssetWithDialog();
}

bool FAssetImportManager::ImportMeshSource(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceDiskPath = ResolveSourcePathOnDisk(SourcePath);
    const FString SourceDiskPathString = ToUtf8GenericPath(SourceDiskPath);
    const std::wstring Ext = ToLowerExtension(SourceDiskPath);
    const bool bSkeletalMesh = Ext == L".fbx" && FMeshAssetManager::IsFbxSkeletalMesh(SourceDiskPathString);
    const wchar_t* Prefix = bSkeletalMesh ? L"SK_" : L"SM_";
    std::filesystem::path DestinationPath = MakeDestinationAssetPath(SourceDiskPathString, L"Meshes", Prefix);
    EnsureUniquePath(DestinationPath);

    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Source classified as %s: source=%s target=%s",
                    bSkeletalMesh ? "USkeletalMesh" : "UStaticMesh",
                    SourceDiskPathString.c_str(), ToUtf8GenericPath(DestinationPath).c_str());

    if (bSkeletalMesh)
    {
        USkeletalMesh* Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
        FSkeletalMesh* MeshData = new FSkeletalMesh();
        TArray<FStaticMaterial> Materials;

        if (!FFbxSkeletalMeshImporter::Import(SourceDiskPathString, *MeshData, Materials))
        {
            delete MeshData;
            UObjectManager::Get().DestroyObject(Mesh);
            UE_LOG_CATEGORY(AssetImport, Error, "[Import] Skeletal mesh conversion failed: %s", SourceDiskPathString.c_str());
            FNotificationManager::Get().AddNotification("Import failed: skeletal FBX conversion failed.", ENotificationType::Error, 5.0f);
            return false;
        }

        MeshData->PathFileName = MakeProjectRelativePath(DestinationPath);
        Mesh->SetFName(FName(FPaths::ToUtf8(DestinationPath.stem().wstring())));
        Mesh->SetStaticMaterials(std::move(Materials));
        Mesh->SetSkeletalMeshAsset(MeshData);
        return SaveImportedObject(DestinationPath, Mesh, OutImportedAssetPath);
    }

    UStaticMesh* Mesh = UObjectManager::Get().CreateObject<UStaticMesh>();
    FStaticMesh* MeshData = new FStaticMesh();
    TArray<FStaticMaterial> Materials;
    FImportOptions Options = FImportOptions::Default();

    const bool bImported = Ext == L".fbx"
        ? FFbxStaticMeshImporter::Import(SourceDiskPathString, *MeshData, Materials)
        : FObjImporter::Import(SourceDiskPathString, Options, *MeshData, Materials);

    if (!bImported)
    {
        delete MeshData;
        UObjectManager::Get().DestroyObject(Mesh);
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Static mesh conversion failed: %s", SourceDiskPathString.c_str());
        FNotificationManager::Get().AddNotification("Import failed: static mesh conversion failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    MeshData->PathFileName = MakeProjectRelativePath(DestinationPath);
    Mesh->SetFName(FName(FPaths::ToUtf8(DestinationPath.stem().wstring())));
    Mesh->SetStaticMaterials(std::move(Materials));
    Mesh->SetStaticMeshAsset(MeshData);
    return SaveImportedObject(DestinationPath, Mesh, OutImportedAssetPath);
}

bool FAssetImportManager::ImportMtlSource(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const FString SourceDiskPathString = ToUtf8GenericPath(ResolveSourcePathOnDisk(SourcePath));
    TArray<FString> GeneratedMaterialAssetPaths;
    if (!FObjImporter::ImportMtl(SourceDiskPathString, &GeneratedMaterialAssetPaths))
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] MTL conversion failed: %s", SourceDiskPathString.c_str());
        FNotificationManager::Get().AddNotification("Import failed: MTL conversion failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = GeneratedMaterialAssetPaths.empty() ? FString() : GeneratedMaterialAssetPaths.front();
    }
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] MTL generated %zu material asset(s).", GeneratedMaterialAssetPaths.size());
    return true;
}

bool FAssetImportManager::ImportTextureSource(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceDiskPath = ResolveSourcePathOnDisk(SourcePath);
    const FString SourceDiskPathString = ToUtf8GenericPath(SourceDiskPath);
    const FString SourceStem = SanitizeAssetName(FPaths::ToUtf8(SourceDiskPath.stem().wstring()));
    const std::filesystem::path TextureAssetDirectory = std::filesystem::path(FPaths::ContentDir()) / L"Textures";

    UTexture2D* Texture = UObjectManager::Get().CreateObject<UTexture2D>();
    Texture->SetFName(FName(FString("T_") + SourceStem));
    Texture->SetSourcePathForAsset(SourceDiskPathString);

    std::filesystem::path DestinationPath = TextureAssetDirectory / (L"T_" + FPaths::ToWide(SourceStem) + L".uasset");
    EnsureUniquePath(DestinationPath);
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Source classified as UTexture2D: source=%s target=%s", SourceDiskPathString.c_str(), ToUtf8GenericPath(DestinationPath).c_str());
    return SaveImportedObject(DestinationPath, Texture, OutImportedAssetPath);
}

bool FAssetImportManager::ImportExistingUAsset(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
    FAssetFileHeader Header;
    if (!FAssetFileSerializer::ReadAssetHeader(SourceAbsolute, Header))
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Invalid .uasset file: %s", ToUtf8GenericPath(SourceAbsolute).c_str());
        FNotificationManager::Get().AddNotification("Import failed: invalid .uasset file.", ENotificationType::Error, 5.0f);
        return false;
    }

    const wchar_t* Category = L"Imported";
    switch (Header.ClassId)
    {
    case EAssetClassId::StaticMesh:
    case EAssetClassId::SkeletalMesh:
        Category = L"Meshes";
        break;
    case EAssetClassId::Material:
        Category = L"Materials";
        break;
    case EAssetClassId::Texture:
        Category = L"Textures";
        break;
    case EAssetClassId::PoseAsset:
        Category = L"Poses";
        break;
    default:
        Category = L"Imported";
        break;
    }

    std::filesystem::path DestinationDirectory = std::filesystem::path(FPaths::ContentDir()) / Category;
    std::filesystem::create_directories(DestinationDirectory);
    std::filesystem::path DestinationPath = DestinationDirectory / SourceAbsolute.filename();
    EnsureUniquePath(DestinationPath);

    FNotificationToast::UpdateProgress("60%: copying .uasset", 0.60f);
    FNotificationManager::Get().AddNotification("Import 60%: copying .uasset", ENotificationType::Info, 2.0f);
    std::filesystem::copy_file(SourceAbsolute, DestinationPath, std::filesystem::copy_options::overwrite_existing);

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = MakeProjectRelativePath(DestinationPath);
    }
    UE_LOG_CATEGORY(AssetImport, Info, "[Import] Existing %s copied into Content: %s", GetAssetClassIdName(Header.ClassId), MakeProjectRelativePath(DestinationPath).c_str());
    return true;
}

bool FAssetImportManager::SaveImportedObject(const std::filesystem::path& DestinationPath, UObject* Object, FString* OutImportedAssetPath)
{
    if (!Object)
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Save failed: null UObject. target=%s", ToUtf8GenericPath(DestinationPath).c_str());
        return false;
    }

    std::filesystem::create_directories(DestinationPath.parent_path());

    FNotificationToast::UpdateProgress("60%: saving .uasset", 0.60f);
    FNotificationManager::Get().AddNotification("Import 60%: saving .uasset", ENotificationType::Info, 2.0f);
    FString Error;
    const bool bSaved = FAssetFileSerializer::SaveObjectToAssetFile(DestinationPath, Object, &Error);
    if (!bSaved)
    {
        UE_LOG_CATEGORY(AssetImport, Error, "[Import] Save .uasset failed: target=%s error=%s", ToUtf8GenericPath(DestinationPath).c_str(), Error.c_str());
        FNotificationManager::Get().AddNotification(Error.empty() ? "Import failed: could not save .uasset." : Error,
                                                    ENotificationType::Error, 5.0f);
        UObjectManager::Get().DestroyObject(Object);
        return false;
    }

    FString RelativePath = MakeProjectRelativePath(DestinationPath);
    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = RelativePath;
    }

    FAssetFileHeader Header;
    const char* ClassName = "UObject";
    if (FAssetFileSerializer::ReadAssetHeader(DestinationPath, Header))
    {
        ClassName = GetAssetClassIdName(Header.ClassId);
    }
    UE_LOG_CATEGORY(AssetImport, Info, "[AssetSave] Saved %s .uasset: %s", ClassName, RelativePath.c_str());
    return true;
}

std::filesystem::path FAssetImportManager::MakeDestinationAssetPath(const FString& SourcePath, const wchar_t* CategoryDir, const wchar_t* Prefix) const
{
    const std::filesystem::path Source(FPaths::ToWide(SourcePath));
    const FString SourceStem = SanitizeAssetName(FPaths::ToUtf8(Source.stem().wstring()));

    std::filesystem::path DestinationDirectory = std::filesystem::path(FPaths::ContentDir()) / CategoryDir;
    return DestinationDirectory / (std::wstring(Prefix) + FPaths::ToWide(SourceStem) + L".uasset");
}

FString FAssetImportManager::MakeProjectRelativePath(const std::filesystem::path& Path) const
{
    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    const std::filesystem::path Normalized = Path.lexically_normal();
    const std::filesystem::path Relative = Normalized.lexically_relative(ProjectRoot);
    if (!Relative.empty() && Relative.native().find(L"..") != 0)
    {
        return FPaths::ToUtf8(Relative.generic_wstring());
    }
    return FPaths::ToUtf8(Normalized.generic_wstring());
}
