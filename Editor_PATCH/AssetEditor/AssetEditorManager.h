#pragma once

#include "AssetEditor/Window/AssetEditorWindow.h"

#include <filesystem>
#include <memory>

class UObject;
class UEditorEngine;
class FRenderer;
class IAssetEditor;
class USkeletalMesh;

class FAssetEditorManager
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer);
    void Shutdown();

    void Tick(float DeltaTime);
    void RenderContent(float DeltaTime);

    bool OpenAssetFromPath(const std::filesystem::path &AssetPath);
    bool OpenSourceFileFromPath(const std::filesystem::path &SourcePath);
    bool OpenLoadedAsset(UObject *Asset, const std::filesystem::path &AssetPath);
    bool OpenAssetWithDialog(void *OwnerWindowHandle = nullptr);
    bool OpenFbxWithDialog(void *OwnerWindowHandle = nullptr);
    bool OpenFbxForPreview(const std::filesystem::path &FbxPath);
    bool ShowAssetEditorWindow();
    bool CreateCameraModifierStackAsset();

    bool SaveActiveEditor();
    void CloseActiveEditor();

    bool IsCapturingInput() const;

    FAssetEditorWindow &GetAssetEditorWindow() { return AssetEditorWindow; }
    const FAssetEditorWindow &GetAssetEditorWindow() const { return AssetEditorWindow; }

  private:
    std::unique_ptr<IAssetEditor> CreateEditorForAsset(UObject *Asset) const;
    USkeletalMesh *CreateDummySkeletalMeshForEditorPreview(const std::filesystem::path &SourcePath) const;

  private:
    UEditorEngine *EditorEngine = nullptr;
    FRenderer     *Renderer = nullptr;

    FAssetEditorWindow AssetEditorWindow;
};
