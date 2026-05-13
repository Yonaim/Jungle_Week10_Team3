#include "PCH/LunaticPCH.h"
#include "AssetTools/AssetImportManager.h"

#include "Common/File/EditorFileUtils.h"
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
#include <fstream>
#include <sstream>

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

    bool CopySourceFileToProject(const FString& SourcePath, const std::filesystem::path& DestinationDirectory, FString& OutProjectRelativePath)
    {
        const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
        if (!std::filesystem::exists(SourceAbsolute))
        {
            return false;
        }

        std::filesystem::create_directories(DestinationDirectory);
        std::filesystem::path DestinationPath = DestinationDirectory / SourceAbsolute.filename();
        EnsureUniquePath(DestinationPath);

        std::filesystem::copy_file(SourceAbsolute, DestinationPath, std::filesystem::copy_options::overwrite_existing);

        const std::filesystem::path ProjectRoot(FPaths::RootDir());
        OutProjectRelativePath = FPaths::ToUtf8(DestinationPath.lexically_relative(ProjectRoot).generic_wstring());
        return true;
    }

    json::JSON ReadJsonFile(const FString& FilePath)
    {
        std::ifstream File(FPaths::ToWide(FilePath));
        if (!File.is_open())
        {
            return json::JSON();
        }

        std::stringstream Buffer;
        Buffer << File.rdbuf();
        return json::JSON::Load(Buffer.str());
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
        .Filter = L"Importable Asset Sources (*.fbx;*.obj;*.mtl;*.mat;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.uasset)\0*.fbx;*.obj;*.mtl;*.mat;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds;*.uasset\0All Files (*.*)\0*.*\0",
        .Title = L"Import Asset",
        .InitialDirectory = FPaths::AssetDir().c_str(),
        .OwnerWindowHandle = EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr,
        .bFileMustExist = true,
        .bPathMustExist = true,
        .bPromptOverwrite = false,
        .bReturnRelativeToProjectRoot = false,
    });

    if (SelectedPath.empty())
    {
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
        FNotificationManager::Get().AddNotification("Import failed: source file not found.", ENotificationType::Error, 5.0f);
        return false;
    }

    const std::wstring Ext = ToLowerExtension(SourceAbsolute);
    bool bResult = false;

    if (Ext == L".fbx" || Ext == L".obj")
    {
        bResult = ImportMeshSource(SourcePath, OutImportedAssetPath);
    }
    else if (Ext == L".mtl")
    {
        bResult = ImportMtlSource(SourcePath, OutImportedAssetPath);
    }
    else if (Ext == L".mat")
    {
        bResult = ImportLegacyMaterialJson(SourcePath, OutImportedAssetPath);
    }
    else if (Ext == L".uasset")
    {
        bResult = ImportExistingUAsset(SourcePath, OutImportedAssetPath);
    }
    else if (UTexture2D::IsSupportedTextureExtension(SourceAbsolute))
    {
        bResult = ImportTextureSource(SourcePath, OutImportedAssetPath);
    }
    else
    {
        FNotificationManager::Get().AddNotification("Import failed: unsupported source file type.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (bResult)
    {
        FMeshAssetManager::ScanMeshAssets();
        FMaterialManager::Get().ScanMaterialAssets();
        UTexture2D::ScanTextureAssets();
        if (EditorEngine)
        {
            EditorEngine->RefreshContentBrowser();
        }

        const FString Message = OutImportedAssetPath && !OutImportedAssetPath->empty()
            ? "Imported asset: " + *OutImportedAssetPath
            : "Imported asset.";
        FNotificationManager::Get().AddNotification(Message, ENotificationType::Success, 3.0f);
    }

    return bResult;
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

    if (bSkeletalMesh)
    {
        USkeletalMesh* Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
        FSkeletalMesh* MeshData = new FSkeletalMesh();
        TArray<FStaticMaterial> Materials;

        if (!FFbxSkeletalMeshImporter::Import(SourceDiskPathString, *MeshData, Materials))
        {
            delete MeshData;
            UObjectManager::Get().DestroyObject(Mesh);
            FNotificationManager::Get().AddNotification("Import failed: skeletal FBX conversion failed.", ENotificationType::Error, 5.0f);
            return false;
        }

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
        FNotificationManager::Get().AddNotification("Import failed: static mesh conversion failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    MeshData->PathFileName = SourceDiskPathString;
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
        FNotificationManager::Get().AddNotification("Import failed: MTL conversion failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = GeneratedMaterialAssetPaths.empty() ? FString() : GeneratedMaterialAssetPaths.front();
    }
    return true;
}

bool FAssetImportManager::ImportLegacyMaterialJson(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const FString SourceDiskPathString = ToUtf8GenericPath(ResolveSourcePathOnDisk(SourcePath));
    json::JSON JsonData = ReadJsonFile(SourceDiskPathString);
    if (JsonData.IsNull())
    {
        FNotificationManager::Get().AddNotification("Import failed: material JSON could not be read.", ENotificationType::Error, 5.0f);
        return false;
    }

    std::filesystem::path DestinationPath = MakeDestinationAssetPath(SourceDiskPathString, L"Materials", L"M_");
    EnsureUniquePath(DestinationPath);
    const FString DestinationRelativePath = MakeProjectRelativePath(DestinationPath);
    JsonData[MatKeys::PathFileName] = DestinationRelativePath;

    UMaterial* Material = FMaterialManager::Get().CreateMaterialAssetFromJson(DestinationRelativePath, JsonData);
    if (!Material)
    {
        FNotificationManager::Get().AddNotification("Import failed: material asset creation failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = DestinationRelativePath;
    }
    return true;
}

bool FAssetImportManager::ImportTextureSource(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceDiskPath = ResolveSourcePathOnDisk(SourcePath);
    const FString SourceDiskPathString = ToUtf8GenericPath(SourceDiskPath);
    const FString SourceStem = SanitizeAssetName(FPaths::ToUtf8(SourceDiskPath.stem().wstring()));
    const std::filesystem::path TextureDirectory = std::filesystem::path(FPaths::ContentDir()) / L"Textures" / FPaths::ToWide(SourceStem);

    FString CopiedTexturePath;
    if (!CopySourceFileToProject(SourceDiskPathString, TextureDirectory, CopiedTexturePath))
    {
        FNotificationManager::Get().AddNotification("Import failed: texture copy failed.", ENotificationType::Error, 5.0f);
        return false;
    }

    UTexture2D* Texture = UObjectManager::Get().CreateObject<UTexture2D>();
    Texture->SetFName(FName(FString("T_") + SourceStem));
    Texture->SetSourcePathForAsset(CopiedTexturePath);

    std::filesystem::path DestinationPath = TextureDirectory / (L"T_" + FPaths::ToWide(SourceStem) + L".uasset");
    EnsureUniquePath(DestinationPath);
    return SaveImportedObject(DestinationPath, Texture, OutImportedAssetPath);
}

bool FAssetImportManager::ImportExistingUAsset(const FString& SourcePath, FString* OutImportedAssetPath)
{
    const std::filesystem::path SourceAbsolute = ResolveSourcePathOnDisk(SourcePath);
    FAssetFileHeader Header;
    if (!FAssetFileSerializer::ReadAssetHeader(SourceAbsolute, Header))
    {
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
    default:
        Category = L"Imported";
        break;
    }

    std::filesystem::path DestinationDirectory = std::filesystem::path(FPaths::ContentDir()) / Category;
    std::filesystem::create_directories(DestinationDirectory);
    std::filesystem::path DestinationPath = DestinationDirectory / SourceAbsolute.filename();
    EnsureUniquePath(DestinationPath);

    std::filesystem::copy_file(SourceAbsolute, DestinationPath, std::filesystem::copy_options::overwrite_existing);

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = MakeProjectRelativePath(DestinationPath);
    }
    return true;
}

bool FAssetImportManager::SaveImportedObject(const std::filesystem::path& DestinationPath, UObject* Object, FString* OutImportedAssetPath)
{
    if (!Object)
    {
        return false;
    }

    std::filesystem::create_directories(DestinationPath.parent_path());

    FString Error;
    const bool bSaved = FAssetFileSerializer::SaveObjectToAssetFile(DestinationPath, Object, &Error);
    if (!bSaved)
    {
        FNotificationManager::Get().AddNotification(Error.empty() ? "Import failed: could not save .uasset." : Error,
                                                    ENotificationType::Error, 5.0f);
        UObjectManager::Get().DestroyObject(Object);
        return false;
    }

    if (OutImportedAssetPath)
    {
        *OutImportedAssetPath = MakeProjectRelativePath(DestinationPath);
    }
    return true;
}

std::filesystem::path FAssetImportManager::MakeDestinationAssetPath(const FString& SourcePath, const wchar_t* CategoryDir, const wchar_t* Prefix) const
{
    const std::filesystem::path Source(FPaths::ToWide(SourcePath));
    const FString SourceStem = SanitizeAssetName(FPaths::ToUtf8(Source.stem().wstring()));

    std::filesystem::path DestinationDirectory = std::filesystem::path(FPaths::ContentDir()) / CategoryDir / FPaths::ToWide(SourceStem);
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
