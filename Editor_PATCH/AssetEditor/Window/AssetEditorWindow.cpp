#include "AssetEditor/Window/AssetEditorWindow.h"

#include "AssetEditor/AssetEditorManager.h"
#include "AssetEditor/IAssetEditor.h"
#include "EditorEngine.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"

namespace
{
    constexpr ImVec4 AssetEditorSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
    constexpr ImVec4 AssetEditorFrameBg = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.0f);
    constexpr ImVec4 AssetEditorBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
} // namespace

void FAssetEditorWindow::Initialize(UEditorEngine *InEditorEngine, FAssetEditorManager *InOwnerManager)
{
    EditorEngine = InEditorEngine;
    OwnerManager = InOwnerManager;
}

void FAssetEditorWindow::Shutdown()
{
    while (TabManager.HasOpenTabs())
    {
        TabManager.CloseActiveTab();
    }

    bOpen = false;
    bVisible = false;
    bCapturingInput = false;

    EditorEngine = nullptr;
    OwnerManager = nullptr;
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
    if (!IsOpen())
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

void FAssetEditorWindow::RenderWindowContents(float DeltaTime)
{
    ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, AssetEditorFrameBg);
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, AssetEditorSurface);
    ImGui::PushStyleColor(ImGuiCol_Border, AssetEditorBorder);

    bool bWindowOpen = bOpen;
    ImGuiID DockspaceId = 0;
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

        if (TabManager.HasOpenTabs())
        {
            DockspaceId = ImGui::GetID("##AssetEditorDockSpace");
            RenderDockSpace();
        }
        else
        {
            RenderEmptyState();
        }
    }
    ImGui::End();

    if (TabManager.HasOpenTabs())
    {
        TabManager.Render(DeltaTime, DockspaceId);
    }

    ImGui::PopStyleColor(3);
    bOpen = bWindowOpen;
    bVisible = bWindowOpen;
}

void FAssetEditorWindow::RenderDockSpace()
{
    const ImGuiID DockspaceId = ImGui::GetID("##AssetEditorDockSpace");
    ImGui::DockSpace(DockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
}

void FAssetEditorWindow::RenderEmptyState()
{
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 32.0f);

    const char *Title = "No asset is open";
    const char *Hint = "Open a .uasset or .fbx from File, or double-click it in the Content Browser.";

    const float AvailableWidth = ImGui::GetContentRegionAvail().x;
    const ImVec2 TitleSize = ImGui::CalcTextSize(Title);
    const ImVec2 HintSize = ImGui::CalcTextSize(Hint);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (AvailableWidth - TitleSize.x) * 0.5f);
    ImGui::TextDisabled("%s", Title);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (AvailableWidth - HintSize.x) * 0.5f);
    ImGui::TextDisabled("%s", Hint);

    ImGui::Spacing();
    const float ButtonWidth = 140.0f;
    const float ButtonGap = 8.0f;
    const float TotalButtonWidth = ButtonWidth * 2.0f + ButtonGap;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - TotalButtonWidth) * 0.5f);
    if (ImGui::Button("Open Asset...", ImVec2(ButtonWidth, 0.0f)) && OwnerManager)
    {
        OwnerManager->OpenAssetWithDialog(EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr);
    }
    ImGui::SameLine(0.0f, ButtonGap);
    if (ImGui::Button("Open FBX...", ImVec2(ButtonWidth, 0.0f)) && OwnerManager)
    {
        OwnerManager->OpenFbxWithDialog(EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr);
    }
}


void FAssetEditorWindow::BuildFileMenu()
{
    if (ImGui::MenuItem("Open Asset...") && OwnerManager)
    {
        OwnerManager->OpenAssetWithDialog(EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr);
    }

    if (ImGui::MenuItem("Open FBX...") && OwnerManager)
    {
        OwnerManager->OpenFbxWithDialog(EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr);
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
