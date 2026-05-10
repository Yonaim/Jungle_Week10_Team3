#pragma once

#include "AssetEditor/Tabs/AssetEditorTabManager.h"

#include <memory>

class UEditorEngine;
class FRenderer;
class IAssetEditor;
class FWindowsWindow;

class FAssetEditorWindow
{
  public:
    bool Create(UEditorEngine *InEditorEngine, FRenderer *InRenderer);
    void Destroy();

    void Show();
    void Hide();

    bool IsOpen() const;

    bool OpenEditorTab(std::unique_ptr<IAssetEditor> Editor);
    bool ActivateTabByAssetPath(const std::filesystem::path &AssetPath);
    bool SaveActiveTab();
    void CloseActiveTab();

    void Tick(float DeltaTime);
    void Render(float DeltaTime);

    bool IsCapturingInput() const;

  private:
    bool EnsureNativeWindow();

  private:
    std::unique_ptr<FWindowsWindow> NativeWindow;

    UEditorEngine *EditorEngine = nullptr;
    FRenderer     *Renderer = nullptr;

    FAssetEditorTabManager TabManager;

    bool bOpen = false;
    bool bCapturingInput = false;
};
