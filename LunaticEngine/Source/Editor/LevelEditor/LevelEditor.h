#pragma once

#include "LevelEditor/Selection/SelectionManager.h"
#include "LevelEditor/PIE/LevelPIEManager.h"
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
    void Init(UEditorEngine *InEditorEngine, FWindowsWindow *InWindow, FRenderer &InRenderer);
    void Shutdown();

    FSelectionManager &GetSelectionManager() { return SelectionManager; }

    const FSelectionManager &GetSelectionManager() const { return SelectionManager; }

    FLevelViewportLayout &GetViewportLayout() { return ViewportLayout; }

    const FLevelViewportLayout &GetViewportLayout() const { return ViewportLayout; }

    FOverlayStatSystem &GetOverlayStatSystem() { return OverlayStatSystem; }

    const FOverlayStatSystem &GetOverlayStatSystem() const { return OverlayStatSystem; }

    FLevelPIEManager& GetPIEManager() { return PIEManager; }

    const FLevelPIEManager& GetPIEManager() const { return PIEManager; }

  private:
    UEditorEngine       *EditorEngine = nullptr;
    FSelectionManager    SelectionManager;
    FLevelViewportLayout ViewportLayout;
    FOverlayStatSystem   OverlayStatSystem;
    FLevelPIEManager     PIEManager;
};
