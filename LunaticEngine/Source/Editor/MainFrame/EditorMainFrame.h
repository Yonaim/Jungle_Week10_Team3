#pragma once

#include "MainFrame/EditorMainMenuBar.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;
class IEditorMenuProvider;
struct ImGuiContext;
struct ImFont;

class FEditorMainFrame
{
  public:
    void Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine);
    void Release();

    void BeginFrame();
    void RenderMainMenuBar(IEditorMenuProvider *MenuProvider);
    void RenderCommonOverlays();
    void EndFrame();

    void UpdateInputState(bool bMouseOverViewport, bool bAssetEditorCapturingInput, bool bPIEPopupOpen);

    bool HasBlockingOverlayOpen() const;
    void ShowProjectSettings() { bShowProjectSettings = true; }
    void ToggleShortcutOverlay() { bShowShortcutOverlay = !bShowShortcutOverlay; }
    void ToggleCreditsOverlay() { bShowCreditsOverlay = !bShowCreditsOverlay; }

  private:
    void MakeCurrentContext() const;
    void RenderProjectSettingsWindow();
    void RenderShortcutOverlay();
    void RenderCreditsOverlay();

  private:
    FWindowsWindow *Window = nullptr;
    UEditorEngine *EditorEngine = nullptr;
    ImGuiContext *MainImGuiContext = nullptr;
    ImFont *TitleBarFont = nullptr;
    ImFont *WindowControlIconFont = nullptr;
    FEditorMainMenuBar MainMenuBar;
    bool bShowProjectSettings = false;
    bool bShowShortcutOverlay = false;
    bool bShowCreditsOverlay = false;
};
