#pragma once

class FWindowsWindow;
class UEditorEngine;
class IEditorMenuProvider;
struct ImFont;

struct FEditorMainMenuBarContext
{
    FWindowsWindow *Window = nullptr;
    UEditorEngine *EditorEngine = nullptr;
    IEditorMenuProvider *MenuProvider = nullptr;
    ImFont *TitleBarFont = nullptr;
    ImFont *WindowControlIconFont = nullptr;
    bool *bShowProjectSettings = nullptr;
    bool *bShowShortcutOverlay = nullptr;
    bool *bShowCreditsOverlay = nullptr;
};

class FEditorMainMenuBar
{
  public:
    void Render(const FEditorMainMenuBarContext &Context);
};
