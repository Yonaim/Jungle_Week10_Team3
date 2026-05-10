#include "AssetTools/AssetImportManager.h"

#include "Common/File/EditorFileUtils.h"
#include "EditorEngine.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Materials/MaterialManager.h"
#include "Texture/Texture2D.h"
#include <filesystem>

void FAssetImportManager::Init(UEditorEngine* InEditorEngine)
{
    EditorEngine = InEditorEngine;
}

void FAssetImportManager::Shutdown()
{
    EditorEngine = nullptr;
}

bool FAssetImportManager::ImportMaterialWithDialog()
{
    if (!EditorEngine)
    {
        return false;
    }

    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"Material Files (*.mat)\0*.mat\0All Files (*.*)\0*.*\0",
        .Title = L"Import Material",
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

    FString ImportedPath;
    if (!ImportAssetFile(SelectedPath, L"Asset\\Materials\\Imported", ImportedPath))
    {
        return false;
    }

    FMaterialManager::Get().ScanMaterialAssets();
    EditorEngine->RefreshContentBrowser();
    return true;
}

bool FAssetImportManager::ImportTextureWithDialog()
{
    if (!EditorEngine)
    {
        return false;
    }

    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"Texture Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds\0All Files (*.*)\0*.*\0",
        .Title = L"Import Texture",
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

    FString ImportedPath;
    if (!ImportAssetFile(SelectedPath, L"Asset\\Textures\\Imported", ImportedPath))
    {
        return false;
    }

    ID3D11Device* Device = EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();
    UTexture2D::LoadFromFile(ImportedPath, Device);
    UTexture2D::ScanTextureAssets();
    EditorEngine->RefreshContentBrowser();
    return true;
}

bool FAssetImportManager::ImportAssetFile(const FString& SourcePath, const std::wstring& RelativeDestinationDir, FString& OutImportedRelativePath)
{
    if (SourcePath.empty())
    {
        return false;
    }

    const std::filesystem::path ProjectRoot(FPaths::RootDir());
    const std::filesystem::path SourceAbsolute = std::filesystem::path(FPaths::ToWide(SourcePath)).lexically_normal();
    if (!std::filesystem::exists(SourceAbsolute))
    {
        return false;
    }

    const std::filesystem::path RelativeToRoot = SourceAbsolute.lexically_relative(ProjectRoot);
    const bool bAlreadyInProject = !RelativeToRoot.empty() && RelativeToRoot.native().find(L"..") != 0;
    if (bAlreadyInProject)
    {
        OutImportedRelativePath = FPaths::ToUtf8(RelativeToRoot.generic_wstring());
        return true;
    }

    const std::filesystem::path DestinationDir = ProjectRoot / RelativeDestinationDir;
    FPaths::CreateDir(DestinationDir.wstring());

    std::filesystem::path DestinationPath = DestinationDir / SourceAbsolute.filename();
    int32 Suffix = 1;
    while (std::filesystem::exists(DestinationPath))
    {
        DestinationPath = DestinationDir /
            (SourceAbsolute.stem().wstring() + L"_" + std::to_wstring(Suffix++) + SourceAbsolute.extension().wstring());
    }

    std::filesystem::copy_file(SourceAbsolute, DestinationPath, std::filesystem::copy_options::overwrite_existing);
    OutImportedRelativePath = FPaths::ToUtf8(DestinationPath.lexically_relative(ProjectRoot).generic_wstring());
    return true;
}
