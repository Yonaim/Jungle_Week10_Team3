#include "PCH/LunaticPCH.h"
#include "AssetEditor/Tabs/AssetEditorTabManager.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Tabs/EditorDocumentTabBar.h"

#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/Tabs/AssetEditorTab.h"

#include "ImGui/imgui.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    std::filesystem::path NormalizeAssetPath(const std::filesystem::path &Path)
    {
        return Path.empty() ? Path : Path.lexically_normal();
    }
}

bool FAssetEditorTabManager::OpenTab(std::unique_ptr<IAssetEditor> Editor)
{
    if (!Editor)
    {
        return false;
    }

    const std::filesystem::path NewAssetPath = NormalizeAssetPath(Editor->GetAssetPath());
    if (!NewAssetPath.empty() && ActivateTabByAssetPath(NewAssetPath))
    {
        return true;
    }

    Tabs.push_back(std::make_unique<FAssetEditorTab>(std::move(Editor)));
    SetActiveTabIndex(static_cast<int32>(Tabs.size()) - 1);
    return true;
}

bool FAssetEditorTabManager::ActivateTabByAssetPath(const std::filesystem::path &AssetPath)
{
    const std::filesystem::path NormalizedPath = NormalizeAssetPath(AssetPath);
    for (int32 Index = 0; Index < static_cast<int32>(Tabs.size()); ++Index)
    {
        if (!Tabs[Index])
        {
            continue;
        }

        if (NormalizeAssetPath(Tabs[Index]->GetAssetPath()) == NormalizedPath)
        {
            SetActiveTabIndex(Index);
            return true;
        }
    }

    return false;
}

void FAssetEditorTabManager::CloseTab(int32 TabIndex)
{
    if (TabIndex < 0 || TabIndex >= static_cast<int32>(Tabs.size()))
    {
        return;
    }

    if (Tabs[TabIndex] && Tabs[TabIndex]->GetEditor())
    {
        Tabs[TabIndex]->GetEditor()->Close();
    }

    Tabs.erase(Tabs.begin() + TabIndex);

    if (Tabs.empty())
    {
        ActiveTabIndex = -1;
        return;
    }

    if (ActiveTabIndex == TabIndex)
    {
        SetActiveTabIndex((std::min)(TabIndex, static_cast<int32>(Tabs.size()) - 1));
    }
    else if (TabIndex < ActiveTabIndex)
    {
        ActiveTabIndex = (std::max)(0, ActiveTabIndex - 1);
    }
    else if (ActiveTabIndex >= static_cast<int32>(Tabs.size()))
    {
        SetActiveTabIndex(static_cast<int32>(Tabs.size()) - 1);
    }
}

void FAssetEditorTabManager::CloseActiveTab()
{
    CloseTab(ActiveTabIndex);
}

bool FAssetEditorTabManager::SaveActiveTab()
{
    IAssetEditor *Editor = GetActiveEditor();
    return Editor ? Editor->Save() : false;
}

void FAssetEditorTabManager::Tick(float DeltaTime)
{
    // 비활성 탭은 패널을 렌더링하지 않는다.
    // 다만 에셋 에디터별 background tick이 필요한 경우를 고려해 Tick은 유지한다.
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (Tab)
        {
            Tab->Tick(DeltaTime);
        }
    }
}

void FAssetEditorTabManager::InvalidateEditorLayouts()
{
    for (const std::unique_ptr<FAssetEditorTab>& Tab : Tabs)
    {
        if (Tab && Tab->GetEditor())
        {
            Tab->GetEditor()->InvalidateDockLayout();
        }
    }
}

bool FAssetEditorTabManager::RenderDocumentTabBar()
{
    CompactInvalidTabs();
    if (Tabs.empty())
    {
        return false;
    }

    std::vector<FEditorDocumentTabBar::FTabDesc> TabDescs;
    TabDescs.reserve(Tabs.size());
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (!Tab || !Tab->GetEditor())
        {
            continue;
        }

        FEditorDocumentTabBar::FTabDesc Desc;
        Desc.Label = Tab->GetTitle();
        Desc.Tooltip = FPaths::ToUtf8(Tab->GetAssetPath().wstring());
        Desc.bDirty = Tab->GetEditor()->IsDirty();
        Desc.IconKey = Tab->GetEditor()->GetDocumentTabIconKey();
        Desc.IconTint = Tab->GetEditor()->GetDocumentTabIconTint();
        TabDescs.push_back(std::move(Desc));
    }

    FEditorDocumentTabBar::FRenderResult Result =
        FEditorDocumentTabBar::Render("AssetEditorDocumentTabBar", TabDescs, ActiveTabIndex);

    if (Result.SelectedIndex != ActiveTabIndex)
    {
        SetActiveTabIndex(Result.SelectedIndex);
    }

    if (Result.CloseRequestedIndex >= 0)
    {
        CloseTab(Result.CloseRequestedIndex);
    }

    return true;
}

void FAssetEditorTabManager::Render(float DeltaTime, ImGuiID DockspaceId)
{
    CompactInvalidTabs();
    RenderActiveTab(DeltaTime, DockspaceId);
}

void FAssetEditorTabManager::RenderActiveTab(float DeltaTime, ImGuiID DockspaceId)
{
    FAssetEditorTab *Tab = GetActiveTab();
    if (!Tab || !Tab->GetEditor())
    {
        return;
    }

    IAssetEditor *Editor = Tab->GetEditor();
    if (Editor->UsesExternalPanels())
    {
        // SkeletalMeshEditor처럼 에디터 내부 영역이 실제 패널 단위로 나뉘는 경우.
        // 다중 문서 탭 구조에서는 반드시 ActiveTab 하나만 외부 패널을 렌더링해야 한다.
        Editor->RenderPanels(DeltaTime, DockspaceId);
        return;
    }

    bool bOpen = true;
    const std::string StableId = std::string("AssetEditorTab_") + std::to_string(ActiveTabIndex);
    const std::string TabTitle = Tab->GetTitle();
    FPanelDesc PanelDesc;
    PanelDesc.DisplayName = TabTitle.c_str();
    PanelDesc.StableId = StableId.c_str();
    PanelDesc.IconKey = "Editor.Icon.Panel.Asset";
    PanelDesc.DockspaceId = DockspaceId;
    PanelDesc.bClosable = true;
    PanelDesc.bOpen = &bOpen;
    PanelDesc.WindowFlags = ImGuiWindowFlags_NoCollapse;

    if (FPanel::Begin(PanelDesc))
    {
        Tab->Render(DeltaTime);
    }
    FPanel::End();

    if (!bOpen)
    {
        CloseActiveTab();
    }
}

void FAssetEditorTabManager::CompactInvalidTabs()
{
    for (int32 Index = 0; Index < static_cast<int32>(Tabs.size());)
    {
        if (!Tabs[Index] || !Tabs[Index]->GetEditor())
        {
            CloseTab(Index);
            continue;
        }
        ++Index;
    }
}

FAssetEditorTab *FAssetEditorTabManager::GetActiveTab() const
{
    if (ActiveTabIndex < 0 || ActiveTabIndex >= static_cast<int32>(Tabs.size()) || !Tabs[ActiveTabIndex])
    {
        return nullptr;
    }
    return Tabs[ActiveTabIndex].get();
}

bool FAssetEditorTabManager::SetActiveTabIndex(int32 NewIndex)
{
    if (NewIndex < 0 || NewIndex >= static_cast<int32>(Tabs.size()))
    {
        return false;
    }

    if (ActiveTabIndex == NewIndex)
    {
        return true;
    }

    ActiveTabIndex = NewIndex;
    if (Tabs[ActiveTabIndex] && Tabs[ActiveTabIndex]->GetEditor())
    {
        // DockspaceId는 그대로인데 ActiveEditor만 바뀌는 경우가 있으므로,
        // 새로 활성화된 에디터가 자기 기본 layout을 다시 보장하게 한다.
        Tabs[ActiveTabIndex]->GetEditor()->InvalidateDockLayout();
    }
    return true;
}

bool FAssetEditorTabManager::HasOpenTabs() const
{
    return !Tabs.empty();
}

int32 FAssetEditorTabManager::GetTabCount() const
{
    return static_cast<int32>(Tabs.size());
}

bool FAssetEditorTabManager::IsCapturingInput() const
{
    IAssetEditor *Editor = GetActiveEditor();
    return Editor && Editor->IsCapturingInput();
}

IAssetEditor *FAssetEditorTabManager::GetActiveEditor() const
{
    FAssetEditorTab *Tab = GetActiveTab();
    return Tab ? Tab->GetEditor() : nullptr;
}

FEditorViewportClient *FAssetEditorTabManager::GetActiveViewportClient() const
{
    IAssetEditor *Editor = GetActiveEditor();
    return Editor ? Editor->GetActiveViewportClient() : nullptr;
}

void FAssetEditorTabManager::CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const
{
    // 비활성 document tab의 preview viewport는 화면에 보이지 않으므로 렌더/입력 대상에서 제외한다.
    IAssetEditor *Editor = GetActiveEditor();
    if (Editor)
    {
        Editor->CollectViewportClients(OutClients);
    }
}
