#pragma once

#include "AssetEditor/IAssetEditor.h"

#include <filesystem>

class UObject;
class UEditorEngine;
class FRenderer;
class USkeletalMesh;

class FSkeletalMeshEditor final : public IAssetEditor
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer) override;
    bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) override;
    void Close() override;
    bool Save() override;

    void RenderContent(float DeltaTime) override;

    bool IsDirty() const override { return false; }
    bool IsCapturingInput() const override { return bCapturingInput; }
    const char *GetEditorName() const override { return "SkeletalMeshEditor"; }
    const std::filesystem::path &GetAssetPath() const override { return EditingAssetPath; }

  private:
    UEditorEngine        *EditorEngine = nullptr;
    FRenderer            *Renderer = nullptr;
    USkeletalMesh        *EditingAsset = nullptr;
    std::filesystem::path EditingAssetPath;
    bool                  bOpen = false;
    bool                  bCapturingInput = false;
};
