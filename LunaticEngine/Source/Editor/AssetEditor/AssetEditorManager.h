#pragma once

#include "AssetEditor/Window/AssetEditorWindow.h"

#include <filesystem>
#include <memory>

class UObject;
class UEditorEngine;
class FRenderer;
class IAssetEditor;

class FAssetEditorManager
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer);
    void Shutdown();

    void Tick(float DeltaTime);
    void Render(float DeltaTime);

    bool OpenAssetFromPath(const std::filesystem::path &AssetPath);
    bool OpenLoadedAsset(UObject *Asset, const std::filesystem::path &AssetPath);
    bool OpenAssetWithDialog(void *OwnerWindowHandle = nullptr);
    bool CreateCameraModifierStackAsset();

    bool SaveActiveEditor();
    void CloseActiveEditor();

    bool IsCapturingInput() const;

    FAssetEditorWindow &GetAssetEditorWindow() { return AssetEditorWindow; }
    const FAssetEditorWindow &GetAssetEditorWindow() const { return AssetEditorWindow; }

  private:
    std::unique_ptr<IAssetEditor> CreateEditorForAsset(UObject *Asset) const;

  private:
    UEditorEngine *EditorEngine = nullptr;
    FRenderer     *Renderer = nullptr;

    FAssetEditorWindow AssetEditorWindow;
};
