#pragma once

#include "LevelEditor/Settings/LevelEditorSettings.h"
#include "LevelEditor/UI/Debug/ShadowMapDebugPanel.h"
#include "LevelEditor/UI/Panels/ContentBrowser/ContentBrowser.h"
#include "LevelEditor/UI/Panels/LevelConsolePanel.h"
#include "LevelEditor/UI/Panels/LevelDetailsPanel.h"
#include "LevelEditor/UI/Panels/LevelOutlinerPanel.h"
#include "LevelEditor/UI/Panels/LevelPlaceActorsPanel.h"
#include "LevelEditor/UI/Panels/LevelStatPanel.h"
#include "MainFrame/EditorMenuProvider.h"


class FRenderer;
class UEditorEngine;
class FWindowsWindow;
class FLevelEditor;
class FEditorMainFrame;

class FLevelEditorWindow : public IEditorMenuProvider
{
  public:
    void  Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine, FLevelEditor *InLevelEditor,
                 FEditorMainFrame *InMainFrame);
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
    void  FlushPendingMenuAction();
    void  BuildFileMenu() override;
    void  BuildEditMenu() override;
    void  BuildWindowMenu() override;
    void  BuildCustomMenus() override;
    FString GetFrameTitle() const override;
    FString GetFrameTitleTooltip() const override;

  private:
    enum class EPendingMenuAction
    {
        None,
        NewScene,
        OpenScene,
        SaveScene,
        SaveSceneAs,
        NewUAsset,
        OpenUAsset,
        ImportMaterial,
        ImportTexture,
        CookCurrentScene,
        CookAllScenes,
        PackageRelease,
        PackageShipping,
        PackageDemo,
    };

    void HandleGlobalShortcuts();
    void PackageGameBuild(const char *BatFileName);
    void CookCurrentScene();

    FWindowsWindow                                   *Window = nullptr;
    UEditorEngine                                    *EditorEngine = nullptr;
    FLevelEditor                                     *LevelEditor = nullptr;
    FEditorMainFrame                                 *MainFrame = nullptr;
    FLevelConsolePanel                                ConsolePanel;
    FLevelDetailsPanel                                DetailsPanel;
    FLevelOutlinerPanel                               OutlinerPanel;
    FLevelPlaceActorsPanel                            PlaceActorsPanel;
    FLevelStatPanel                                   StatPanel;
    FContentBrowser                                   ContentBrowser;
    FShadowMapDebugPanel                              ShadowMapDebugPanel;
    bool                                              bShowPanelList = false;
    bool                                              bHideEditorWindows = false;
    bool                                              bHasSavedPanelVisibility = false;
    bool                                              bSavedShowPanelList = false;
    EPendingMenuAction                                PendingMenuAction = EPendingMenuAction::None;
    FLevelEditorSettings::FLevelEditorPanelVisibility SavedPanelVisibility{};
};
