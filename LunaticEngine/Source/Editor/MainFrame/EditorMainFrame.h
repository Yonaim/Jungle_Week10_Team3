#pragma once

#include "AssetEditor/CameraModifierStack/CameraModifierStackEditor.h"
#include "LevelEditor/UI/Debug/ShadowMapDebugPanel.h"
#include "LevelEditor/UI/Panels/ContentBrowser/ContentBrowser.h"
#include "LevelEditor/UI/Panels/LevelConsolePanel.h"
#include "LevelEditor/UI/Panels/LevelDetailsPanel.h"
#include "LevelEditor/UI/Panels/LevelOutlinerPanel.h"
#include "LevelEditor/UI/Panels/LevelPlaceActorsPanel.h"
#include "LevelEditor/UI/Panels/LevelStatPanel.h"
#include "Settings/EditorSettings.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;
struct ImFont;

class FEditorMainFrame
{
  public:
    void Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine);
    void Release();
    void Render(float DeltaTime);
    void Update();
    void SaveToSettings() const;
    void HideEditorWindows();
    void ShowEditorWindows();
    void SetShowEditorOnlyComponents(bool bEnable)
    {
        DetailsWidget.SetShowEditorOnlyComponents(bEnable);
    }
    bool IsShowingEditorOnlyComponents() const
    {
        return DetailsWidget.IsShowingEditorOnlyComponents();
    }
    void HideEditorWindowsForPIE();
    void RestoreEditorWindowsAfterPIE();
    void RefreshContentBrowser()
    {
        ContentBrowserWidget.Refresh();
    }
    void SetContentBrowserIconSize(float Size)
    {
        ContentBrowserWidget.SetIconSize(Size);
    }
    float GetContentBrowserIconSize() const
    {
        return ContentBrowserWidget.GetIconSize();
    }
    bool IsAssetEditorCapturingInput() const
    {
        return AssetEditorWidget.IsCapturingInput();
    }

  private:
    void RenderMainMenuBar();
    void RenderProjectSettingsWindow();
    void RenderShortcutOverlay();
    void RenderCreditsOverlay();
    void HandleGlobalShortcuts();
    void PackageGameBuild(const char *BatFileName);
    void CookCurrentScene();

    FWindowsWindow *Window = nullptr;
    UEditorEngine *EditorEngine = nullptr;
    ImFont *TitleBarFont = nullptr;
    ImFont *WindowControlIconFont = nullptr;
    FEditorConsoleWidget ConsoleWidget;
    FEditorDetailsWidget DetailsWidget;
    FEditorOutlinerWidget OutlinerWidget;
    FEditorPlaceActorsWidget PlaceActorsWidget;
    FEditorStatWidget StatWidget;
    FEditorContentBrowserWidget ContentBrowserWidget;
    FAssetEditorWidget AssetEditorWidget;
    EditorShadowMapDebugWidget ShadowMapDebugWidget;
    bool bShowWidgetList = false;
    bool bShowShortcutOverlay = false;
    bool bShowCreditsOverlay = false;
    bool bShowProjectSettings = false;
    bool bHideEditorWindows = false;
    bool bHasSavedUIVisibility = false;
    bool bSavedShowWidgetList = false;
    FEditorSettings::FUIVisibility SavedUIVisibility{};
};
