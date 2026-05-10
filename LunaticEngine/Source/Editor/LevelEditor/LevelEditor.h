#pragma once

#include "LevelEditor/Selection/SelectionManager.h"
#include "LevelEditor/Viewport/LevelViewportLayout.h"
#include "LevelEditor/Subsystem/OverlayStatSystem.h"

class UEditorEngine;
class FWindowsWindow;
class FRenderer;
class FEditorViewportClient;
class FLevelEditorViewportClient;

class FLevelEditor
{
public:
    FSelectionManager& GetSelectionManager()
    {
        return SelectionManager;
    }

    const FSelectionManager& GetSelectionManager() const
    {
        return SelectionManager;
    }

    FLevelViewportLayout& GetViewportLayout()
    {
        return ViewportLayout;
    }

    const FLevelViewportLayout& GetViewportLayout() const
    {
        return ViewportLayout;
    }

    FOverlayStatSystem& GetOverlayStatSystem()
    {
        return OverlayStatSystem;
    }

    const FOverlayStatSystem& GetOverlayStatSystem() const
    {
        return OverlayStatSystem;
    }

private:
    FSelectionManager SelectionManager;
    FLevelViewportLayout ViewportLayout;
    FOverlayStatSystem OverlayStatSystem;
};
