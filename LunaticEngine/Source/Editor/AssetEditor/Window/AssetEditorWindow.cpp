#include "AssetEditor/Window/AssetEditorWindow.h"

#include "AssetEditor/AssetEditorManager.h"
#include "AssetEditor/IAssetEditor.h"
#include "EditorEngine.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "Common/UI/Panels/PanelTitleUtils.h"

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
        // Asset Editor도 Level Editor와 같은 panel chrome / surface 색을 사용한다.
        // 개별 패널의 body 색은 FPanel::Begin()에서 공통으로 적용한다.
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImGui::ColorConvertU32ToFloat4(PanelTitleUtils::GetDockTabBarGapColor()));
        TabManager.Render(DeltaTime, DockspaceId);
        ImGui::PopStyleColor();

        bCapturingInput = TabManager.IsCapturingInput();
        return;
    }

    // 탭이 없으면 아무 panel도 띄우지 않는다.
    // 사용자는 Level Editor의 Content Browser에서 .fbx / .uasset을 열거나,
    // File 메뉴의 Open UAsset / Open FBX 진입점으로 에셋 패널을 추가한다.
    bCapturingInput = false;
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
        if (EditorEngine)
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
