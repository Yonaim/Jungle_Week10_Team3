#pragma once

#include "LevelEditor/History/LevelEditorHistoryManager.h"
#include "LevelEditor/Selection/SelectionManager.h"
#include "LevelEditor/PIE/LevelPIEManager.h"
#include "LevelEditor/Scene/LevelSceneManager.h"
#include "LevelEditor/Subsystem/OverlayStatSystem.h"
#include "LevelEditor/Viewport/LevelViewportLayout.h"

class UEditorEngine;
class FWindowsWindow;
class FRenderer;
class FEditorViewportClient;
class FLevelEditorViewportClient;

class FLevelEditor
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FWindowsWindow *InWindow, FRenderer &InRenderer);
    void Shutdown();
    void Tick(float DeltaTime);

    FSelectionManager &GetSelectionManager() { return SelectionManager; }

    const FSelectionManager &GetSelectionManager() const { return SelectionManager; }

    FLevelViewportLayout &GetViewportLayout() { return ViewportLayout; }

    const FLevelViewportLayout &GetViewportLayout() const { return ViewportLayout; }

    FOverlayStatSystem &GetOverlayStatSystem() { return OverlayStatSystem; }

    const FOverlayStatSystem &GetOverlayStatSystem() const { return OverlayStatSystem; }

    FLevelPIEManager& GetPIEManager() { return PIEManager; }

    const FLevelPIEManager& GetPIEManager() const { return PIEManager; }

    FLevelEditorHistoryManager& GetHistoryManager() { return HistoryManager; }

    const FLevelEditorHistoryManager& GetHistoryManager() const { return HistoryManager; }

    FLevelSceneManager& GetSceneManager() { return SceneManager; }

    const FLevelSceneManager& GetSceneManager() const { return SceneManager; }

  private:
    UEditorEngine       *EditorEngine = nullptr;
    FSelectionManager    SelectionManager;
    FLevelViewportLayout ViewportLayout;
    FOverlayStatSystem   OverlayStatSystem;
    FLevelPIEManager     PIEManager;
    FLevelEditorHistoryManager HistoryManager;
    FLevelSceneManager   SceneManager;
};
