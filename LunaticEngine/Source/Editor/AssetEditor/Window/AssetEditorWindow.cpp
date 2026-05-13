#include "PCH/LunaticPCH.h"
#include "AssetEditor/Window/AssetEditorWindow.h"

#include "AssetEditor/AssetEditorManager.h"
#include "AssetEditor/IAssetEditor.h"
#include "EditorEngine.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Common/UI/Tabs/EditorDocumentTabBar.h"

#include <algorithm>


namespace
{
    const float AssetDocumentTabBarHeight = FEditorDocumentTabBar::GetHeight();
    constexpr float EditorWindowOuterPadding = 6.0f;
    constexpr float EditorTitleBarHeight = 42.0f;
}

void FAssetEditorWindow::Initialize(UEditorEngine *InEditorEngine, FAssetEditorManager *InOwnerManager)
{
    EditorEngine = InEditorEngine;
    OwnerManager = InOwnerManager;
}

void FAssetEditorWindow::Shutdown()
{
    TabManager.CloseAllTabs(false);

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

    // 같은 메인 DockSpace를 Level Editor / Asset Editor가 번갈아 쓰므로,
    // Level Editor로 돌아갔다가 다시 FBX를 열면 기존 DockBuilder node가 Level layout로 바뀐 상태일 수 있다.
    // Asset Editor가 다시 보일 때 외부 패널형 에디터(SkeletalMeshEditor 등)의 layout을 강제로 재빌드한다.
    TabManager.InvalidateEditorLayouts();
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

bool FAssetEditorWindow::CloseActiveTab(bool bPromptForDirty)
{
    const bool bClosed = TabManager.CloseActiveTab(bPromptForDirty);
    if (bClosed && !TabManager.HasOpenTabs())
    {
        Hide();
    }
    return bClosed;
}

bool FAssetEditorWindow::CloseAllTabs(bool bPromptForDirty, void *OwnerWindowHandle)
{
    TabManager.SetClosePromptOwnerWindowHandle(OwnerWindowHandle);
    const bool bClosed = TabManager.CloseAllTabs(bPromptForDirty);
    if (bClosed)
    {
        Hide();
    }
    return bClosed;
}

bool FAssetEditorWindow::HasDirtyTabs() const
{
    return TabManager.HasDirtyTabs();
}

bool FAssetEditorWindow::ConfirmCloseAllTabs(void *OwnerWindowHandle) const
{
    const_cast<FAssetEditorTabManager &>(TabManager).SetClosePromptOwnerWindowHandle(OwnerWindowHandle);
    return TabManager.ConfirmCloseAllTabs();
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

void FAssetEditorWindow::RenderContent(float DeltaTime, ImGuiID DockspaceId)
{
    if (!IsOpen())
    {
        bCapturingInput = false;
        return;
    }

    // 별도 OS 창 / 별도 ImGui window를 만들지 않고, Level Editor의 메인 DockSpace 안에
    // 에셋 에디터 패널들을 직접 추가한다.
    // - 일반 에셋 에디터(CameraModifierStack 등)는 하나의 dockable panel로 감싸서 렌더링한다.
    // - SkeletalMeshEditor는 UsesExternalPanels()를 통해 Toolbar / Preview Viewport /
    //   Skeleton Tree / Details를 각각 독립 panel로 렌더링한다.
    if (TabManager.HasOpenTabs())
    {
        RenderDocumentTabBar();

        // Asset Editor도 Level Editor와 같은 panel chrome / surface 색을 사용한다.
        // 개별 패널의 body 색은 FPanel::Begin()에서 공통으로 적용한다.
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImGui::ColorConvertU32ToFloat4(PanelTitleUtils::GetDockTabBarGapColor()));
        TabManager.Render(DeltaTime, DockspaceId);
        ImGui::PopStyleColor();

        if (!TabManager.HasOpenTabs())
        {
            Hide();
            if (EditorEngine)
            {
                EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
            }
            return;
        }

        bCapturingInput = TabManager.IsCapturingInput();
        return;
    }

    // 탭이 없으면 아무 panel도 띄우지 않는다.
    // 사용자는 Level Editor의 Content Browser에서 .fbx / .uasset을 열거나,
    // File 메뉴의 Open UAsset 진입점으로 에셋 패널을 추가한다.
    bCapturingInput = false;
}

void FAssetEditorWindow::RenderDocumentTabBar()
{
    if (!IsOpen() || !TabManager.HasOpenTabs())
    {
        return;
    }

    const ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    if (!MainViewport)
    {
        return;
    }

    const ImVec2 BarPos(MainViewport->Pos.x + EditorWindowOuterPadding,
                        MainViewport->Pos.y + EditorWindowOuterPadding + EditorTitleBarHeight);
    const ImVec2 BarSize((std::max)(0.0f, MainViewport->Size.x - EditorWindowOuterPadding * 2.0f), AssetDocumentTabBarHeight);
    if (BarSize.x <= 1.0f || BarSize.y <= 1.0f)
    {
        return;
    }

    ImGui::SetNextWindowPos(BarPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(BarSize, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(MainViewport->ID);

    const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.035f, 0.040f, 1.0f));
    if (ImGui::Begin("##AssetEditorDocumentTabBar", nullptr, Flags))
    {
        TabManager.SetClosePromptOwnerWindowHandle(EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr);
        TabManager.RenderDocumentTabBar();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

void FAssetEditorWindow::RenderEmptyState(ImGuiID DockspaceId)
{
    (void)DockspaceId;
}

bool FAssetEditorWindow::IsCapturingInput() const
{
    return IsOpen() && (bCapturingInput || TabManager.IsCapturingInput());
}


FEditorViewportClient *FAssetEditorWindow::GetActiveViewportClient() const
{
    if (!IsOpen())
    {
        return nullptr;
    }

    return TabManager.GetActiveViewportClient();
}

void FAssetEditorWindow::CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const
{
    if (!IsOpen())
    {
        return;
    }

    TabManager.CollectViewportClients(OutClients);
}


void FAssetEditorWindow::BuildFileMenu()
{
    if (ImGui::MenuItem("Back to Level Editor") && EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Open Asset...") && OwnerManager)
    {
        OwnerManager->OpenAssetWithDialog(EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Save Active Tab", "Ctrl+S"))
    {
        SaveActiveTab();
    }

    if (ImGui::MenuItem("Close Active Tab", "Ctrl+W"))
    {
        CloseActiveTab(true);
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Close Window"))
    {
        if (CloseAllTabs(true, EditorEngine && EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr) && EditorEngine)
        {
            EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
        }
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
        return FString("Asset Editor | ") + ActiveEditor->GetEditorName();
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
            return FString("Asset Editor | ") + ActiveEditor->GetEditorName() + " | " + FPaths::ToUtf8(AssetPath.wstring());
        }
    }

    return GetFrameTitle();
}
