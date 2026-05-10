#include "AssetEditor/AssetEditorManager.h"

#include "Common/File/EditorFileUtils.h"
#include "Core/Notification.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

void FAssetEditorManager::Init(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;

    CameraModifierStackEditor.Init(EditorEngine, Renderer);
    SkeletalMeshEditor.Init(EditorEngine, Renderer);
}

void FAssetEditorManager::Shutdown()
{
    CloseActiveEditor();

    SkeletalMeshEditor.Shutdown();
    CameraModifierStackEditor.Shutdown();

    Renderer = nullptr;
    EditorEngine = nullptr;
}

void FAssetEditorManager::Render(float DeltaTime)
{
    if (ActiveEditor && ActiveEditor->IsOpen())
    {
        ActiveEditor->Render(DeltaTime);
    }
}

bool FAssetEditorManager::OpenAssetFromPath(const std::filesystem::path &AssetPath)
{
    UObject *LoadedAsset = nullptr;

    const std::filesystem::path Ext = AssetPath.extension();

    if (Ext == L".uasset")
    {
        FString Error;
        LoadedAsset = FAssetFileSerializer::LoadAssetFromFile(AssetPath, &Error);

        if (!LoadedAsset)
        {
            FNotificationManager::Get().AddNotification(Error.empty() ? "Failed to load asset." : Error, ENotificationType::Error, 5.0f);
            return false;
        }

        if (OpenLoadedAsset(LoadedAsset, AssetPath))
        {
            return true;
        }

        UObjectManager::Get().DestroyObject(LoadedAsset);
        return false;
    }

    FNotificationManager::Get().AddNotification("Unsupported asset type.", ENotificationType::Info, 3.0f);

    return false;
}

bool FAssetEditorManager::OpenLoadedAsset(UObject *Asset, const std::filesystem::path &AssetPath)
{
    if (!Asset)
    {
        return false;
    }

    IAssetEditor *TargetEditor = ResolveEditorForAsset(Asset);
    if (!TargetEditor)
    {
        FNotificationManager::Get().AddNotification("No editor registered for this asset.", ENotificationType::Info, 3.0f);
        return false;
    }

    if (ActiveEditor && ActiveEditor != TargetEditor)
    {
        ActiveEditor->Close();
    }

    ActiveEditor = TargetEditor;
    return ActiveEditor->OpenAsset(Asset, AssetPath);
}

bool FAssetEditorManager::OpenAssetWithDialog(void *OwnerWindowHandle)
{
    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"Asset Files (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0",
        .Title = L"Open Asset",
        .InitialDirectory = FPaths::AssetDir().c_str(),
        .OwnerWindowHandle = OwnerWindowHandle,
        .bFileMustExist = true,
        .bPathMustExist = true,
        .bPromptOverwrite = false,
        .bReturnRelativeToProjectRoot = false,
    });

    if (SelectedPath.empty())
    {
        return false;
    }

    return OpenAssetFromPath(std::filesystem::path(FPaths::ToWide(SelectedPath)));
}

bool FAssetEditorManager::CreateCameraModifierStackAsset()
{
    CloseActiveEditor();
    ActiveEditor = &CameraModifierStackEditor;
    return CameraModifierStackEditor.CreateCameraShakeAsset();
}

IAssetEditor *FAssetEditorManager::ResolveEditorForAsset(UObject *Asset)
{
    if (CameraModifierStackEditor.CanEdit(Asset))
    {
        return &CameraModifierStackEditor;
    }

    if (SkeletalMeshEditor.CanEdit(Asset))
    {
        return &SkeletalMeshEditor;
    }

    return nullptr;
}

bool FAssetEditorManager::SaveActiveEditor() { return ActiveEditor ? ActiveEditor->Save() : false; }

void FAssetEditorManager::CloseActiveEditor()
{
    if (ActiveEditor)
    {
        ActiveEditor->Close();
        ActiveEditor = nullptr;
    }
}

bool FAssetEditorManager::IsCapturingInput() const { return ActiveEditor && ActiveEditor->IsOpen() && ActiveEditor->IsCapturingInput(); }
