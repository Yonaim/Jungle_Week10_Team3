#include "AssetEditor/Window/AssetEditorWindow.h"

#include "AssetEditor/AssetEditorManager.h"
#include "AssetEditor/IAssetEditor.h"
#include "Common/ImGui/EditorImGuiSystem.h"
#include "Core/Notification.h"
#include "EditorEngine.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>

#include <cstdint>

namespace
{
    constexpr wchar_t AssetEditorWindowClassName[] = L"LunaticAssetEditorWindowClass";
    constexpr LONG AssetEditorWindowStyle = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_VISIBLE;

    constexpr ImVec4 AssetEditorSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
    constexpr ImVec4 AssetEditorFrameBg = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.0f);
    constexpr ImVec4 AssetEditorBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);

    int GetResizeBorderForWindow(HWND hWnd)
    {
        if (!hWnd)
        {
            return 0;
        }

        const UINT Dpi = GetDpiForWindow(hWnd);
        return GetSystemMetricsForDpi(SM_CXFRAME, Dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, Dpi);
    }
} // namespace

bool FAssetEditorWindow::Create(UEditorEngine *InEditorEngine, FRenderer *InRenderer, FAssetEditorManager *InOwnerManager)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;
    OwnerManager = InOwnerManager;
    ImGuiSystem = InEditorEngine ? &InEditorEngine->GetImGuiSystem() : nullptr;

    bOpen = true;
    bVisible = true;
    return true;
}

void FAssetEditorWindow::Destroy()
{
    while (TabManager.HasOpenTabs())
    {
        TabManager.CloseActiveTab();
    }

    bOpen = false;
    bVisible = false;
    bCapturingInput = false;

    EditorEngine = nullptr;
    Renderer = nullptr;
    OwnerManager = nullptr;
    ImGuiSystem = nullptr;
}

void FAssetEditorWindow::Show()
{
    bOpen = true;
    bVisible = true;
}

void FAssetEditorWindow::Hide()
{
    bVisible = false;
    bCapturingInput = false;
}

bool FAssetEditorWindow::IsOpen() const
{
    return bOpen && bVisible;
}

bool FAssetEditorWindow::OpenEditorTab(std::unique_ptr<IAssetEditor> Editor)
{
    if (!Editor)
    {
        return false;
    }

    const bool bResult = TabManager.OpenTab(std::move(Editor));
    if (bResult)
    {
        Show();
    }
    return bResult;
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
    if (!IsOpen())
    {
        bCapturingInput = false;
        return;
    }

    TabManager.Tick(DeltaTime);
}

void FAssetEditorWindow::RenderContent(float DeltaTime)
{
    if (!IsOpen() || !ImGuiSystem)
    {
        bCapturingInput = false;
        return;
    }

    RenderWindowContents(DeltaTime);
}

bool FAssetEditorWindow::IsCapturingInput() const
{
    return IsOpen() && (bCapturingInput || TabManager.IsCapturingInput());
}

LRESULT CALLBACK FAssetEditorWindow::StaticWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    FAssetEditorWindow *Window = reinterpret_cast<FAssetEditorWindow *>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    if (Msg == WM_NCCREATE)
    {
        CREATESTRUCTW *CreateStruct = reinterpret_cast<CREATESTRUCTW *>(lParam);
        Window = reinterpret_cast<FAssetEditorWindow *>(CreateStruct->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Window));
    }

    return Window ? Window->WindowProc(hWnd, Msg, wParam, lParam) : DefWindowProcW(hWnd, Msg, wParam, lParam);
}

LRESULT FAssetEditorWindow::WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGuiSystem && NativeWindow)
    {
        if (ImGuiSystem->HandleWindowMessage(NativeWindow.get(), hWnd, Msg, static_cast<std::uintptr_t>(wParam),
                                             static_cast<std::intptr_t>(lParam)))
        {
            return 1;
        }
    }

    switch (Msg)
    {
    case WM_NCCALCSIZE:
        return 0;
    case WM_GETMINMAXINFO:
    {
        MONITORINFO MonitorInfo{};
        MonitorInfo.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &MonitorInfo))
        {
            MINMAXINFO *MinMaxInfo = reinterpret_cast<MINMAXINFO *>(lParam);
            const RECT &WorkArea = MonitorInfo.rcWork;
            const RECT &MonitorArea = MonitorInfo.rcMonitor;

            MinMaxInfo->ptMaxPosition.x = WorkArea.left - MonitorArea.left;
            MinMaxInfo->ptMaxPosition.y = WorkArea.top - MonitorArea.top;
            MinMaxInfo->ptMaxSize.x = WorkArea.right - WorkArea.left;
            MinMaxInfo->ptMaxSize.y = WorkArea.bottom - WorkArea.top;
        }
        return 0;
    }
    case WM_NCHITTEST:
    {
        if (!NativeWindow)
        {
            return HTCLIENT;
        }

        POINT Cursor = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        POINT ClientPoint = Cursor;
        ScreenToClient(hWnd, &ClientPoint);
        if (NativeWindow->IsInTitleBarControlRegion(ClientPoint))
        {
            return HTCLIENT;
        }

        RECT WindowRect{};
        GetWindowRect(hWnd, &WindowRect);
        const int ResizeBorderThickness = GetResizeBorderForWindow(hWnd);
        const bool bAllowResize = !NativeWindow->IsResizeLocked() && !IsZoomed(hWnd) && ResizeBorderThickness > 0;

        if (bAllowResize)
        {
            const bool bLeft = Cursor.x >= WindowRect.left && Cursor.x < WindowRect.left + ResizeBorderThickness;
            const bool bRight = Cursor.x < WindowRect.right && Cursor.x >= WindowRect.right - ResizeBorderThickness;
            const bool bBottom = Cursor.y < WindowRect.bottom && Cursor.y >= WindowRect.bottom - ResizeBorderThickness;
            const bool bTop = Cursor.y >= WindowRect.top && Cursor.y < WindowRect.top + ResizeBorderThickness;

            if (bTop && bLeft) return HTTOPLEFT;
            if (bTop && bRight) return HTTOPRIGHT;
            if (bBottom && bLeft) return HTBOTTOMLEFT;
            if (bBottom && bRight) return HTBOTTOMRIGHT;
            if (bTop) return HTTOP;
            if (bLeft) return HTLEFT;
            if (bRight) return HTRIGHT;
            if (bBottom) return HTBOTTOM;
        }

        if (NativeWindow->IsInTitleBarDragRegion(ClientPoint))
        {
            return HTCAPTION;
        }

        return HTCLIENT;
    }
    case WM_CLOSE:
        Hide();
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
        return DefWindowProcW(hWnd, Msg, wParam, lParam);
    case WM_SIZE:
        if (NativeWindow && wParam != SIZE_MINIMIZED)
        {
            const unsigned int Width = LOWORD(lParam);
            const unsigned int Height = HIWORD(lParam);
            NativeWindow->OnResized(Width, Height);
            if (Width == 0 || Height == 0)
            {
                bCapturingInput = false;
                return 0;
            }

            RenderContext.Resize(Width, Height);
        }
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

bool FAssetEditorWindow::CreateNativeWindow()
{
    if (NativeWindow && NativeWindow->GetHWND())
    {
        return true;
    }

    WNDCLASSEXW WindowClass{};
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.lpfnWndProc = StaticWindowProc;
    WindowClass.hInstance = GetModuleHandleW(nullptr);
    WindowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    WindowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    WindowClass.lpszClassName = AssetEditorWindowClassName;

    if (!bWindowClassRegistered)
    {
        const ATOM RegisteredClass = RegisterClassExW(&WindowClass);
        if (RegisteredClass == 0)
        {
            const DWORD Error = GetLastError();
            if (Error != ERROR_CLASS_ALREADY_EXISTS)
            {
                FNotificationManager::Get().AddNotification("Failed to register asset editor window class.",
                                                            ENotificationType::Error, 4.0f);
                return false;
            }
        }
        bWindowClassRegistered = true;
    }

    HWND WindowHandle = CreateWindowExW(0, AssetEditorWindowClassName, L"Lunatic Asset Editor",
                                        AssetEditorWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, 1440, 900, nullptr,
                                        nullptr, GetModuleHandleW(nullptr), this);
    if (!WindowHandle)
    {
        FNotificationManager::Get().AddNotification("Failed to create asset editor window.", ENotificationType::Error, 4.0f);
        return false;
    }

    NativeWindow = std::make_unique<FWindowsWindow>();
    NativeWindow->Initialize(WindowHandle);
    NativeWindow->SetResizeLocked(false);
    if (Renderer && RenderContext.Create(WindowHandle, Renderer->GetFD3DDevice().GetDevice(),
                                         Renderer->GetFD3DDevice().GetDeviceContext()))
    {
        return true;
    }

    FNotificationManager::Get().AddNotification("Failed to create asset editor render context.", ENotificationType::Error, 4.0f);
    DestroyNativeWindow();
    return false;
}

void FAssetEditorWindow::DestroyNativeWindow()
{
    if (NativeWindow && NativeWindow->GetHWND())
    {
        HWND WindowHandle = NativeWindow->GetHWND();
        SetWindowLongPtrW(WindowHandle, GWLP_USERDATA, 0);
        DestroyWindow(WindowHandle);
    }
    NativeWindow.reset();
}

void FAssetEditorWindow::RenderWindowContents(float DeltaTime)
{
    ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, AssetEditorFrameBg);
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, AssetEditorSurface);
    ImGui::PushStyleColor(ImGuiCol_Border, AssetEditorBorder);

    bool bWindowOpen = bOpen;
    const ImGuiWindowFlags HostFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("Asset Editor##AssetEditorRoot", &bWindowOpen, HostFlags))
    {
        bCapturingInput = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
                          ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                BuildFileMenu();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                BuildEditMenu();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window"))
            {
                BuildWindowMenu();
                ImGui::EndMenu();
            }
            BuildCustomMenus();
            ImGui::EndMenuBar();
        }

        if (ImGui::BeginTabBar("##AssetEditorTabs", ImGuiTabBarFlags_Reorderable))
        {
            TabManager.Render(DeltaTime);
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(3);
    bOpen = bWindowOpen;
    bVisible = bWindowOpen;
}


void FAssetEditorWindow::BuildFileMenu()
{
    if (ImGui::MenuItem("Open Asset...") && OwnerManager)
    {
        OwnerManager->OpenAssetWithDialog(NativeWindow ? NativeWindow->GetHWND() : nullptr);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Save Active Tab", "Ctrl+S"))
    {
        SaveActiveTab();
    }

    if (ImGui::MenuItem("Close Active Tab", "Ctrl+W"))
    {
        CloseActiveTab();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Close Window"))
    {
        Hide();
    }
}

void FAssetEditorWindow::BuildEditMenu()
{
    if (IAssetEditor *ActiveEditor = TabManager.GetActiveEditor())
    {
        ActiveEditor->BuildEditMenu();
    }
}

void FAssetEditorWindow::BuildWindowMenu()
{
    if (IAssetEditor *ActiveEditor = TabManager.GetActiveEditor())
    {
        ActiveEditor->BuildWindowMenu();
    }
}

void FAssetEditorWindow::BuildCustomMenus()
{
    if (IAssetEditor *ActiveEditor = TabManager.GetActiveEditor())
    {
        ActiveEditor->BuildCustomMenus();
    }
}

FString FAssetEditorWindow::GetFrameTitle() const
{
    if (IAssetEditor *ActiveEditor = TabManager.GetActiveEditor())
    {
        return ActiveEditor->GetEditorName();
    }

    return "Asset Editor";
}

FString FAssetEditorWindow::GetFrameTitleTooltip() const
{
    if (IAssetEditor *ActiveEditor = TabManager.GetActiveEditor())
    {
        const std::filesystem::path &AssetPath = ActiveEditor->GetAssetPath();
        if (!AssetPath.empty())
        {
            return FPaths::ToUtf8(AssetPath.wstring());
        }
    }

    return GetFrameTitle();
}
