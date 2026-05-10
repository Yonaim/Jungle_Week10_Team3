#pragma once

#include <filesystem>

class FWindowsWindow;
class FD3DDevice;
struct ImGuiContext;
struct ImFont;

class FEditorImGuiSystem
{
  public:
    bool Initialize(FWindowsWindow *InMainWindow, FD3DDevice *InMainDevice);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void SetActiveWindow(FWindowsWindow *InWindow);
    bool HandleWindowMessage(FWindowsWindow *InWindow, void *hWnd, unsigned int Msg, uintptr_t wParam, intptr_t lParam);

    void MakeCurrentContext() const;

    ImGuiContext *GetContext() const { return Context; }
    ImFont *GetTitleBarFont() const { return TitleBarFont; }
    ImFont *GetWindowControlIconFont() const { return WindowControlIconFont; }
    FWindowsWindow *GetMainWindow() const { return MainWindow; }
    FD3DDevice *GetMainDevice() const { return MainDevice; }

  private:
    void ApplyEditorColorTheme();
    void ApplyEditorTabStyle();
    void LoadFonts();

  private:
    ImGuiContext *Context = nullptr;
    FWindowsWindow *MainWindow = nullptr;
    FD3DDevice *MainDevice = nullptr;
    ImFont *TitleBarFont = nullptr;
    ImFont *WindowControlIconFont = nullptr;
    FWindowsWindow *ActiveWindow = nullptr;
    bool bInitialized = false;
};
