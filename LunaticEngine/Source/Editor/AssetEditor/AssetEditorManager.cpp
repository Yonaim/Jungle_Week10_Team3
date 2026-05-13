#include "PCH/LunaticPCH.h"
#include "AssetEditor/AssetEditorManager.h"

#include "AssetEditor/CameraModifierStack/CameraModifierStackEditor.h"
#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"
#include "AssetEditor/Window/AssetEditorWindow.h"
#include "Common/File/EditorFileUtils.h"
#include "Core/Notification.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "EditorEngine.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <cwctype>


namespace
{
std::wstring ToLowerExtension(const std::filesystem::path &Path)
{
    std::wstring Extension = Path.extension().wstring();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](wchar_t Ch) {
        return static_cast<wchar_t>(std::towlower(Ch));
    });
    return Extension;
}


} // namespace

void FAssetEditorManager::Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;
}

void FAssetEditorManager::Shutdown()
{
    AssetEditorWindow.Shutdown();
    Renderer = nullptr;
    EditorEngine = nullptr;
}

void FAssetEditorManager::Tick(float DeltaTime)
{
    AssetEditorWindow.Tick(DeltaTime);
}

void FAssetEditorManager::RenderContent(float DeltaTime, ImGuiID DockspaceId)
{
    AssetEditorWindow.RenderContent(DeltaTime, DockspaceId);
}

bool FAssetEditorManager::OpenAssetFromPath(const std::filesystem::path &AssetPath)
{
    const std::wstring Extension = ToLowerExtension(AssetPath);
    if (Extension != L".uasset")
    {
        FNotificationManager::Get().AddNotification("Asset Editor can only open .uasset files. Use Import Asset first.", ENotificationType::Info, 4.0f);
        return false;
    }

    UObject *LoadedAsset = nullptr;
    FString Error;
    LoadedAsset = FAssetFileSerializer::LoadObjectFromAssetFile(AssetPath, &Error);

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

bool FAssetEditorManager::OpenLoadedAsset(UObject *Asset, const std::filesystem::path &AssetPath)
{
    if (!Asset)
    {
        return false;
    }

    const std::filesystem::path NormalizedPath = AssetPath.lexically_normal();

    if (AssetEditorWindow.ActivateTabByAssetPath(NormalizedPath))
    {
        AssetEditorWindow.Show();
        if (EditorEngine)
        {
            EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
        }
        return true;
    }

    std::unique_ptr<IAssetEditor> Editor = CreateEditorForAsset(Asset);
    if (!Editor)
    {
        FNotificationManager::Get().AddNotification("No editor registered for this asset.", ENotificationType::Info, 3.0f);
        return false;
    }

    Editor->Initialize(EditorEngine, Renderer);
    if (!Editor->OpenAsset(Asset, NormalizedPath))
    {
        Editor->Close();
        return false;
    }

    AssetEditorWindow.Initialize(EditorEngine, this);

    AssetEditorWindow.OpenEditorTab(std::move(Editor));
    AssetEditorWindow.Show();
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
    return true;
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

bool FAssetEditorManager::ShowAssetEditorWindow()
{
    AssetEditorWindow.Initialize(EditorEngine, this);
    AssetEditorWindow.Show();
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
    return true;
}

bool FAssetEditorManager::CreateCameraModifierStackAsset()
{
    std::unique_ptr<FCameraModifierStackEditor> Editor = std::make_unique<FCameraModifierStackEditor>();
    Editor->Initialize(EditorEngine, Renderer);
    if (!Editor->CreateCameraShakeAsset())
    {
        Editor->Close();
        return false;
    }

    AssetEditorWindow.Initialize(EditorEngine, this);

    AssetEditorWindow.OpenEditorTab(std::move(Editor));
    AssetEditorWindow.Show();
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
    return true;
}

bool FAssetEditorManager::SaveActiveEditor()
{
    return AssetEditorWindow.SaveActiveTab();
}

void FAssetEditorManager::CloseActiveEditor()
{
    AssetEditorWindow.CloseActiveTab();
    if (EditorEngine && !AssetEditorWindow.IsOpen())
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
    }
}

bool FAssetEditorManager::IsCapturingInput() const
{
    return AssetEditorWindow.IsCapturingInput();
}


FEditorViewportClient *FAssetEditorManager::GetActiveViewportClient() const
{
    return AssetEditorWindow.GetActiveViewportClient();
}

void FAssetEditorManager::CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const
{
    AssetEditorWindow.CollectViewportClients(OutClients);
}

std::unique_ptr<IAssetEditor> FAssetEditorManager::CreateEditorForAsset(UObject *Asset) const
{
    if (Cast<UCameraModifierStackAssetData>(Asset))
    {
        return std::make_unique<FCameraModifierStackEditor>();
    }

    if (Cast<USkeletalMesh>(Asset))
    {
        return std::make_unique<FSkeletalMeshEditor>();
    }

    return nullptr;
}
