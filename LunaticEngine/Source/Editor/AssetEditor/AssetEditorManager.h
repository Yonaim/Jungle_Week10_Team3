#pragma once

#include <filesystem>

#include "AssetEditor/CameraModifierStack/CameraModifierStackEditor.h"
#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"

class UObject;
class UEditorEngine;
class FRenderer;

class FAssetEditorManager
{
  public:
    void Init(UEditorEngine *InEditorEngine, FRenderer *InRenderer);
    void Shutdown();

    void Render(float DeltaTime);

    bool OpenAssetFromPath(const std::filesystem::path &AssetPath);
    bool OpenLoadedAsset(UObject *Asset, const std::filesystem::path &AssetPath);
    bool OpenAssetWithDialog(void *OwnerWindowHandle = nullptr);
    bool CreateCameraModifierStackAsset();

    bool SaveActiveEditor();
    void CloseActiveEditor();

    bool IsCapturingInput() const;

    IAssetEditor *GetActiveEditor() const { return ActiveEditor; }

  private:
    IAssetEditor *ResolveEditorForAsset(UObject *Asset);

  private:
    UEditorEngine *EditorEngine = nullptr;
    FRenderer     *Renderer = nullptr;

    FCameraModifierStackEditor CameraModifierStackEditor;
    FSkeletalMeshEditor        SkeletalMeshEditor;

    IAssetEditor *ActiveEditor = nullptr;
};
