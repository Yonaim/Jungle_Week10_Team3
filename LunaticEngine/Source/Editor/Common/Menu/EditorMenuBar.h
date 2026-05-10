#pragma once

#include <functional>

class FWindowsWindow;
class UEditorEngine;
class IEditorMenuProvider;
struct ImFont;

struct FEditorMenuBarContext
{
    const char *Id = "##EditorMenuBar";

    FWindowsWindow      *Window = nullptr;
    UEditorEngine       *EditorEngine = nullptr;
    IEditorMenuProvider *MenuProvider = nullptr;

    ImFont *TitleBarFont = nullptr;
    ImFont *WindowControlIconFont = nullptr;

    bool bShowProjectSettingsMenu = false;

    std::function<void()> OnMinimizeWindow;
    std::function<void()> OnToggleMaximizeWindow;
    std::function<void()> OnCloseWindow;
    std::function<void()> OnOpenProjectSettings;
    std::function<void()> OnToggleShortcutOverlay;
    std::function<void()> OnOpenCredits;
};

class FEditorMenuBar
{
  public:
    void Render(const FEditorMenuBarContext &Context);
};
