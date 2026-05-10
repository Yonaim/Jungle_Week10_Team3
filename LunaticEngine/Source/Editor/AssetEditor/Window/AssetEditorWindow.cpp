#include "AssetEditor/Window/AssetEditorWindow.h"

#include "AssetEditor/IAssetEditor.h"
#include "Common/UI/EditorPanelTitleUtils.h"
#include "Core/Notification.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);

namespace
{
    constexpr wchar_t AssetEditorWindowClassName[] = L"LunaticAssetEditorWindowClass";

    constexpr ImVec4 AssetEditorSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
    constexpr ImVec4 AssetEditorFrameBg = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.0f);
    constexpr ImVec4 AssetEditorBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
} // namespace

bool FAssetEditorWindow::Create(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;

    if (!CreateNativeWindow())
    {
        return false;
    }

    bOpen = true;
    Show();
    return true;
}

void FAssetEditorWindow::Destroy()
{
    while (TabManager.HasOpenTabs())
    {
        TabManager.CloseActiveTab();
    }

    bOpen = false;
    bCapturingInput = false;

    ShutdownImGuiContext();
    if (WindowDevice.GetDevice())
    {
        WindowDevice.Release();
    }
    DestroyNativeWindow();

    EditorEngine = nullptr;
    Renderer = nullptr;
}

void FAssetEditorWindow::Show()
{
    bOpen = true;
    if (NativeWindow && NativeWindow->GetHWND())
    {
        ShowWindow(NativeWindow->GetHWND(), SW_SHOW);
        BringWindowToTop(NativeWindow->GetHWND());
        SetForegroundWindow(NativeWindow->GetHWND());
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
    return bOpen && NativeWindow && NativeWindow->GetHWND() && IsWindowVisible(NativeWindow->GetHWND()) != FALSE;
}

bool FAssetEditorWindow::OpenEditorTab(std::unique_ptr<IAssetEditor> Editor)
{
    if (!Editor)
    {
        return false;
    }

    if (!NativeWindow && !CreateNativeWindow())
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

void FAssetEditorWindow::Render(float DeltaTime)
{
    if (!IsOpen() || !WindowImGuiContext || !NativeWindow || !NativeWindow->GetHWND())
    {
        bCapturingInput = false;
        return;
    }

    if (NativeWindow->GetWidth() <= 0.0f || NativeWindow->GetHeight() <= 0.0f)
    {
        bCapturingInput = false;
        return;
    }

    ImGuiContext *PreviousContext = ImGui::GetCurrentContext();
    MakeCurrentContext();

    WindowDevice.BeginFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderWindowContents(DeltaTime);

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    WindowDevice.Present();

    ImGui::SetCurrentContext(PreviousContext);
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
    if (WindowImGuiContext)
    {
        ImGuiContext *PreviousContext = ImGui::GetCurrentContext();
        MakeCurrentContext();
        if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
        {
            ImGui::SetCurrentContext(PreviousContext);
            return 1;
        }
        ImGui::SetCurrentContext(PreviousContext);
    }

    switch (Msg)
    {
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

            if (WindowDevice.GetDevice())
            {
                WindowDevice.OnResizeViewport(static_cast<int>(Width), static_cast<int>(Height));
            }
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
                                        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1440, 900, nullptr,
                                        nullptr, GetModuleHandleW(nullptr), this);
    if (!WindowHandle)
    {
        FNotificationManager::Get().AddNotification("Failed to create asset editor window.", ENotificationType::Error, 4.0f);
        return false;
    }

    NativeWindow = std::make_unique<FWindowsWindow>();
    NativeWindow->Initialize(WindowHandle);
    WindowDevice.Create(WindowHandle);
    SetupImGuiContext();
    return true;
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

void FAssetEditorWindow::SetupImGuiContext()
{
    ImGuiContext* PreviousContext = ImGui::GetCurrentContext();
    WindowImGuiContext = ImGui::CreateContext();
    MakeCurrentContext();

    ImGuiIO &IO = ImGui::GetIO();
    IO.IniFilename = "Settings/imgui_asset_editor.ini";
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGuiStyle &Style = ImGui::GetStyle();
    Style.WindowMenuButtonPosition = ImGuiDir_None;
    Style.CurveTessellationTol = (std::max)(Style.CurveTessellationTol, 0.1f);
    Style.CircleTessellationMaxError = (std::max)(Style.CircleTessellationMaxError, 0.1f);

    const FString               FontPath = FResourceManager::Get().ResolvePath(FName("Default.Font.UI"));
    const std::filesystem::path UIFontPath = std::filesystem::path(FPaths::RootDir()) / FPaths::ToWide(FontPath);
    const FString               UIFontPathAbsolute = FPaths::ToUtf8(UIFontPath.lexically_normal().wstring());
    IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
    WindowTitleFont = IO.Fonts->AddFontFromFileTTF(UIFontPathAbsolute.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
    EditorPanelTitleUtils::EnsurePanelChromeIconFontLoaded();

    ImGui_ImplWin32_Init(reinterpret_cast<void *>(NativeWindow->GetHWND()));
    ImGui_ImplDX11_Init(WindowDevice.GetDevice(), WindowDevice.GetDeviceContext());

    ImGui::SetCurrentContext(PreviousContext);
}

void FAssetEditorWindow::ShutdownImGuiContext()
{
    if (!WindowImGuiContext)
    {
        return;
    }

    ImGuiContext* PreviousContext = ImGui::GetCurrentContext();
    MakeCurrentContext();
    EditorPanelTitleUtils::ReleasePanelChromeIconFontForCurrentContext();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(WindowImGuiContext);
    WindowImGuiContext = nullptr;
    WindowTitleFont = nullptr;
    ImGui::SetCurrentContext(PreviousContext);
}

void FAssetEditorWindow::RenderWindowContents(float DeltaTime)
{
    ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    if (!MainViewport || MainViewport->Size.x <= 0.0f || MainViewport->Size.y <= 0.0f)
    {
        bCapturingInput = false;
        return;
    }

    ImGui::SetNextWindowPos(MainViewport->Pos);
    ImGui::SetNextWindowSize(MainViewport->Size);
    ImGui::SetNextWindowViewport(MainViewport->ID);

    ImGuiWindowFlags HostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, AssetEditorFrameBg);
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, AssetEditorSurface);
    ImGui::PushStyleColor(ImGuiCol_Border, AssetEditorBorder);

    if (ImGui::Begin("##AssetEditorRoot", nullptr, HostFlags))
    {
        if (ImGui::BeginMenuBar())
        {
            if (WindowTitleFont)
            {
                ImGui::PushFont(WindowTitleFont);
            }
            ImGui::TextUnformatted("Asset Editor");
            if (WindowTitleFont)
            {
                ImGui::PopFont();
            }

            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Active Tab"))
                {
                    SaveActiveTab();
                }
                if (ImGui::MenuItem("Close Active Tab"))
                {
                    CloseActiveTab();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::DockSpace(ImGui::GetID("##AssetEditorDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (ImGui::Begin("Asset Documents", nullptr,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
        {
            bCapturingInput = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
                              ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            if (ImGui::BeginTabBar("##AssetEditorTabs", ImGuiTabBarFlags_Reorderable))
            {
                TabManager.Render(DeltaTime);
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
    ImGui::End();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}

void FAssetEditorWindow::MakeCurrentContext() const
{
    if (WindowImGuiContext)
    {
        ImGui::SetCurrentContext(WindowImGuiContext);
    }
}
