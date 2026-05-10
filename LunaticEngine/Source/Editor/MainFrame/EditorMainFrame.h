#pragma once

#include "AssetEditor/AssetEditorManager.h"
#include "LevelEditor/UI/Debug/ShadowMapDebugPanel.h"
#include "LevelEditor/UI/Panels/ContentBrowser/ContentBrowser.h"
#include "LevelEditor/UI/Panels/LevelConsolePanel.h"
#include "LevelEditor/UI/Panels/LevelDetailsPanel.h"
#include "LevelEditor/UI/Panels/LevelOutlinerPanel.h"
#include "LevelEditor/UI/Panels/LevelPlaceActorsPanel.h"
#include "LevelEditor/UI/Panels/LevelStatPanel.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;
struct ImFont;

class FEditorMainFrame
{
  public:
    void  Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine);
    void  Release();
    void  Render(float DeltaTime);
    void  Update();
    void  SaveToSettings() const;
    void  HideEditorWindows();
    void  ShowEditorWindows();
    void  SetShowEditorOnlyComponents(bool bEnable) { DetailsPanel.SetShowEditorOnlyComponents(bEnable); }
    bool  IsShowingEditorOnlyComponents() const { return DetailsPanel.IsShowingEditorOnlyComponents(); }
    void  HideEditorWindowsForPIE();
    void  RestoreEditorWindowsAfterPIE();
    void  RefreshContentBrowser() { ContentBrowser.Refresh(); }
    void  SetContentBrowserIconSize(float Size) { ContentBrowser.SetIconSize(Size); }
    float GetContentBrowserIconSize() const { return ContentBrowser.GetIconSize(); }
    bool  IsAssetEditorCapturingInput() const { return AssetEditorManager.IsCapturingInput(); }

  private:
    void RenderMainMenuBar();
    void RenderProjectSettingsWindow();
    void RenderShortcutOverlay();
    void RenderCreditsOverlay();
    void HandleGlobalShortcuts();
    void PackageGameBuild(const char *BatFileName);
    void CookCurrentScene();

    FWindowsWindow                *Window = nullptr;
    UEditorEngine                 *EditorEngine = nullptr;
    ImFont                        *TitleBarFont = nullptr;
    ImFont                        *WindowControlIconFont = nullptr;
    FLevelConsolePanel           ConsolePanel;
    FLevelDetailsPanel           DetailsPanel;
    FLevelOutlinerPanel          OutlinerPanel;
    FLevelPlaceActorsPanel       PlaceActorsPanel;
    FLevelStatPanel              StatPanel;
    FContentBrowser    ContentBrowser;
    FShadowMapDebugPanel     ShadowMapDebugPanel;
    bool                           bShowPanelList = false;
    bool                           bShowShortcutOverlay = false;
    bool                           bShowCreditsOverlay = false;
    bool                           bShowProjectSettings = false;
    bool                           bHideEditorWindows = false;
    bool                           bHasSavedPanelVisibility = false;
    bool                           bSavedShowPanelList = false;
    FLevelEditorSettings::FLevelEditorPanelVisibility SavedPanelVisibility{};

    FAssetEditorManager AssetEditorManager;

  public:
    FAssetEditorManager &GetAssetEditorManager() { return AssetEditorManager; }

    const FAssetEditorManager &GetAssetEditorManager() const { return AssetEditorManager; }
};
