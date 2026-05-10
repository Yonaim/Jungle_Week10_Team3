#pragma once

#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshDetailsPanel.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshPreviewViewport.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletonTreePanel.h"

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

    void Tick(float DeltaTime) override;
    void RenderContent(float DeltaTime) override;
    void BuildCustomMenus() override;

    bool IsDirty() const override { return bDirty; }
    bool IsCapturingInput() const override { return bCapturingInput; }
    const char *GetEditorName() const override { return "Skeletal Mesh"; }
    const std::filesystem::path &GetAssetPath() const override { return EditingAssetPath; }

  private:
    void RenderLayout(float DeltaTime);

  private:
    UEditorEngine        *EditorEngine = nullptr;
    FRenderer            *Renderer = nullptr;
    USkeletalMesh        *EditingAsset = nullptr;
    std::filesystem::path EditingAssetPath;

    FSkeletalMeshEditorState State;
    FSkeletalMeshEditorToolbar Toolbar;
    FSkeletalMeshPreviewViewport PreviewViewport;
    FSkeletonTreePanel SkeletonTreePanel;
    FSkeletalMeshDetailsPanel DetailsPanel;

    bool bOpen = false;
    bool bDirty = false;
    bool bCapturingInput = false;
};
