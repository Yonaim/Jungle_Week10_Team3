#pragma once

#include "AssetEditor/Tabs/AssetEditorTabManager.h"
#include "Common/Menu/EditorMenuProvider.h"

class UEditorEngine;
class FAssetEditorManager;
class IAssetEditor;

class FAssetEditorWindow : public IEditorMenuProvider
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FAssetEditorManager *InOwnerManager);
    void Shutdown();

    void Show();
    void Hide();

    bool IsOpen() const;

    bool OpenEditorTab(std::unique_ptr<IAssetEditor> Editor);
    bool ActivateTabByAssetPath(const std::filesystem::path &AssetPath);
    bool SaveActiveTab();
    void CloseActiveTab();

    void Tick(float DeltaTime);
    void RenderContent(float DeltaTime);

    bool IsCapturingInput() const;

    void BuildFileMenu() override;
    void BuildEditMenu() override;
    void BuildWindowMenu() override;
    void BuildCustomMenus() override;
    FString GetFrameTitle() const override;
    FString GetFrameTitleTooltip() const override;

  private:
    void RenderWindowContents(float DeltaTime);
    void RenderDockSpace();
    void RenderEmptyState();

  private:
    UEditorEngine       *EditorEngine = nullptr;
    FAssetEditorManager *OwnerManager = nullptr;

    FAssetEditorTabManager TabManager;

    bool bOpen = false;
    bool bVisible = false;
    bool bCapturingInput = false;
};
