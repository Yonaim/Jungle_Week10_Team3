#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorDetailsWidget.h"
#include "Editor/UI/EditorOutlinerWidget.h"
#include "Editor/UI/EditorStatWidget.h"
#include "Editor/UI/EditorShadowMapDebugWidget.h"
#include "Editor/UI/EditorPlaceActorsWidget.h"
#include "Editor/UI/ContentBrowser/ContentBrowser.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;

class FEditorMainPanel
{
public:
	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime);
	void Update();
	void SaveToSettings() const;
	void HideEditorWindows();
	void ShowEditorWindows();
	void SetShowEditorOnlyComponents(bool bEnable) { DetailsWidget.SetShowEditorOnlyComponents(bEnable); }
	bool IsShowingEditorOnlyComponents() const { return DetailsWidget.IsShowingEditorOnlyComponents(); }
	void HideEditorWindowsForPIE();
	void RestoreEditorWindowsAfterPIE();
	void RefreshContentBrowser() { ContentBrowserWidget.Refresh(); }
	void SetContentBrowserIconSize(float Size) { ContentBrowserWidget.SetIconSize(Size); }
	float GetContentBrowserIconSize() const { return ContentBrowserWidget.GetIconSize(); }

private:
	void RenderMainMenuBar();
	void RenderShortcutOverlay();
	void HandleGlobalShortcuts();

	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;
	FEditorConsoleWidget ConsoleWidget;
	FEditorDetailsWidget DetailsWidget;
	FEditorOutlinerWidget OutlinerWidget;
	FEditorPlaceActorsWidget PlaceActorsWidget;
	FEditorStatWidget StatWidget;
	FEditorContentBrowserWidget ContentBrowserWidget;
	EditorShadowMapDebugWidget ShadowMapDebugWidget;
	bool bShowWidgetList = false;
	bool bShowShortcutOverlay = false;
	bool bHideEditorWindows = false;
	bool bHasSavedUIVisibility = false;
	bool bSavedShowWidgetList = false;
	FEditorSettings::FUIVisibility SavedUIVisibility{};
};
