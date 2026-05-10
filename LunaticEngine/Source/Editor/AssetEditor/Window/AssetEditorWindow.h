#pragma once

#include "AssetEditor/Tabs/AssetEditorTabManager.h"
#include "Render/Device/D3DDevice.h"

#include <memory>

class UEditorEngine;
class FRenderer;
class IAssetEditor;
class FWindowsWindow;
struct ImGuiContext;
struct ImFont;

class FAssetEditorWindow
{
  public:
    bool Create(UEditorEngine *InEditorEngine, FRenderer *InRenderer);
    void Destroy();

    void Show();
    void Hide();

    bool IsOpen() const;

    bool OpenEditorTab(std::unique_ptr<IAssetEditor> Editor);
    bool ActivateTabByAssetPath(const std::filesystem::path &AssetPath);
    bool SaveActiveTab();
    void CloseActiveTab();

    void Tick(float DeltaTime);
    void Render(float DeltaTime);

    bool IsCapturingInput() const;

  private:
    static LRESULT CALLBACK StaticWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    LRESULT WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

    bool CreateNativeWindow();
    void DestroyNativeWindow();
    void SetupImGuiContext();
    void ShutdownImGuiContext();
    void RenderWindowContents(float DeltaTime);
    void MakeCurrentContext() const;

  private:
    std::unique_ptr<FWindowsWindow> NativeWindow;
    FD3DDevice                      WindowDevice;
    ImGuiContext                   *WindowImGuiContext = nullptr;
    ImFont                         *WindowTitleFont = nullptr;

    UEditorEngine *EditorEngine = nullptr;
    FRenderer     *Renderer = nullptr;

    FAssetEditorTabManager TabManager;

    bool bOpen = false;
    bool bCapturingInput = false;
    bool bWindowClassRegistered = false;
};
