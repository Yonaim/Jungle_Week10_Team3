#pragma once

#include "AssetEditor/Tabs/AssetEditorTabManager.h"
#include "Engine/Render/Window/WindowRenderContext.h"
#include "Common/Menu/EditorMenuProvider.h"
#include "Common/Menu/EditorMenuBar.h"

#include <memory>

class UEditorEngine;
class FRenderer;
class FAssetEditorManager;
class FEditorImGuiSystem;
class IAssetEditor;
class FWindowsWindow;

class FAssetEditorWindow : public IEditorMenuProvider
{
  public:
    bool Create(UEditorEngine *InEditorEngine, FRenderer *InRenderer, FAssetEditorManager *InOwnerManager);
    void Destroy();

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
    static LRESULT CALLBACK StaticWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    LRESULT WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

    bool CreateNativeWindow();
    void DestroyNativeWindow();
    void RenderWindowContents(float DeltaTime);

  private:
    std::unique_ptr<FWindowsWindow> NativeWindow;
    FWindowRenderContext            RenderContext;

    UEditorEngine       *EditorEngine = nullptr;
    FRenderer           *Renderer = nullptr;
    FAssetEditorManager *OwnerManager = nullptr;
    FEditorImGuiSystem  *ImGuiSystem = nullptr;

    FEditorMenuBar MenuBar;

    FAssetEditorTabManager TabManager;

    bool bOpen = false;
    bool bVisible = false;
    bool bCapturingInput = false;
    bool bWindowClassRegistered = false;
};
