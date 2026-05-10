#include "AssetEditor/Window/AssetEditorWindow.h"

#include "AssetEditor/IAssetEditor.h"
#include "Core/Notification.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace
{
    constexpr wchar_t AssetEditorWindowClassName[] = L"LunaticAssetEditorWindowClass";

    LRESULT CALLBACK AssetEditorWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
    {
        if (Msg == WM_CLOSE)
        {
            ShowWindow(hWnd, SW_HIDE);
            return 0;
        }

        return DefWindowProcW(hWnd, Msg, wParam, lParam);
    }
} // namespace

bool FAssetEditorWindow::Create(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;
    if (!EnsureNativeWindow())
    {
        return false;
    }

    bOpen = true;
    return true;
}

void FAssetEditorWindow::Destroy()
{
    Hide();

    while (TabManager.HasOpenTabs())
    {
        TabManager.CloseActiveTab();
    }

    if (NativeWindow && NativeWindow->GetHWND())
    {
        DestroyWindow(NativeWindow->GetHWND());
    }

    NativeWindow.reset();
    Renderer = nullptr;
    EditorEngine = nullptr;
    bCapturingInput = false;
}

void FAssetEditorWindow::Show()
{
    bOpen = true;

    if (NativeWindow && NativeWindow->GetHWND())
    {
        ShowWindow(NativeWindow->GetHWND(), SW_SHOWNOACTIVATE);
    }
}

void FAssetEditorWindow::Hide()
{
    bOpen = false;
    bCapturingInput = false;

    if (NativeWindow && NativeWindow->GetHWND())
    {
        ShowWindow(NativeWindow->GetHWND(), SW_HIDE);
    }
}

bool FAssetEditorWindow::IsOpen() const
{
    return bOpen;
}

bool FAssetEditorWindow::OpenEditorTab(std::unique_ptr<IAssetEditor> Editor)
{
    if (!Editor)
    {
        return false;
    }

    bOpen = true;
    return TabManager.OpenTab(std::move(Editor));
}

bool FAssetEditorWindow::ActivateTabByAssetPath(const std::filesystem::path &AssetPath)
{
    return TabManager.ActivateTabByAssetPath(AssetPath);
}

bool FAssetEditorWindow::SaveActiveTab()
{
    return TabManager.SaveActiveTab();
}

void FAssetEditorWindow::CloseActiveTab()
{
    TabManager.CloseActiveTab();
    if (!TabManager.HasOpenTabs())
    {
        Hide();
    }
}

void FAssetEditorWindow::Tick(float DeltaTime)
{
    if (!bOpen)
    {
        bCapturingInput = false;
        return;
    }

    TabManager.Tick(DeltaTime);
}

void FAssetEditorWindow::Render(float DeltaTime)
{
    if (!bOpen || !TabManager.HasOpenTabs())
    {
        bCapturingInput = false;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(1100.0f, 720.0f), ImGuiCond_FirstUseEver);

    bool bWindowOpen = bOpen;
    if (!ImGui::Begin("Asset Editor Window", &bWindowOpen, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        bOpen = bWindowOpen;
        bCapturingInput = bOpen;
        return;
    }

    bOpen = bWindowOpen;
    bCapturingInput = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
                      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (ImGui::BeginTabBar("##AssetEditorTabs", ImGuiTabBarFlags_Reorderable))
    {
        TabManager.Render(DeltaTime);
        ImGui::EndTabBar();
    }

    ImGui::End();

    if (!bOpen)
    {
        Hide();
    }
}

bool FAssetEditorWindow::IsCapturingInput() const
{
    return bOpen && (bCapturingInput || TabManager.IsCapturingInput());
}

bool FAssetEditorWindow::EnsureNativeWindow()
{
    if (NativeWindow && NativeWindow->GetHWND())
    {
        return true;
    }

    HINSTANCE Instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW WindowClass{};
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.lpfnWndProc = AssetEditorWindowProc;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    WindowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    WindowClass.lpszClassName = AssetEditorWindowClassName;
    RegisterClassExW(&WindowClass);

    HWND WindowHandle = CreateWindowExW(0, AssetEditorWindowClassName, L"Lunatic Asset Editor",
                                        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, nullptr,
                                        nullptr, Instance, nullptr);
    if (!WindowHandle)
    {
        FNotificationManager::Get().AddNotification("Failed to create asset editor native window.", ENotificationType::Error, 4.0f);
        return false;
    }

    NativeWindow = std::make_unique<FWindowsWindow>();
    NativeWindow->Initialize(WindowHandle);
    ShowWindow(WindowHandle, SW_HIDE);
    UpdateWindow(WindowHandle);

    FNotificationManager::Get().AddNotification(
        "Asset editor native window shell created. Multi-window render context hookup is still pending.",
        ENotificationType::Info, 4.0f);
    return true;
}
