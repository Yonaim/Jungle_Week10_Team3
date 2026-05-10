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
    void Init(UEditorEngine *InEditorEngine, FRenderer *InRenderer) override;
    void Shutdown() override;

    bool CanEdit(UObject *Asset) const override;
    bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) override;
    void Close() override;
    bool Save() override;

    void Render(float DeltaTime) override;

    bool IsOpen() const override { return bOpen; }
    bool IsDirty() const override { return false; }
    bool IsCapturingInput() const override { return bCapturingInput; }
    const char *GetEditorName() const override { return "SkeletalMeshEditor"; }

  private:
    UEditorEngine        *EditorEngine = nullptr;
    FRenderer            *Renderer = nullptr;
    USkeletalMesh        *EditingAsset = nullptr;
    std::filesystem::path EditingAssetPath;
    bool                  bOpen = false;
    bool                  bCapturingInput = false;
};
