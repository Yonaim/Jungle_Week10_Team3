#include "AssetEditor/Tabs/AssetEditorTabManager.h"

#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/Tabs/AssetEditorTab.h"

#include "ImGui/imgui.h"

#include <algorithm>
#include <string>

bool FAssetEditorTabManager::OpenTab(std::unique_ptr<IAssetEditor> Editor)
{
    if (!Editor)
    {
        return false;
    }

    Tabs.push_back(std::make_unique<FAssetEditorTab>(std::move(Editor)));
    ActiveTabIndex = static_cast<int32>(Tabs.size()) - 1;
    return true;
}

bool FAssetEditorTabManager::ActivateTabByAssetPath(const std::filesystem::path &AssetPath)
{
    const std::filesystem::path NormalizedPath = AssetPath.lexically_normal();
    for (int32 Index = 0; Index < static_cast<int32>(Tabs.size()); ++Index)
    {
        if (Tabs[Index] && Tabs[Index]->GetAssetPath() == NormalizedPath)
        {
            ActiveTabIndex = Index;
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

    if (ActiveTabIndex >= static_cast<int32>(Tabs.size()))
    {
        ActiveTabIndex = static_cast<int32>(Tabs.size()) - 1;
    }
    else if (TabIndex <= ActiveTabIndex)
    {
        ActiveTabIndex = (std::max)(0, ActiveTabIndex - 1);
    }
}

void FAssetEditorTabManager::CloseActiveTab()
{
    CloseTab(ActiveTabIndex);
}

bool FAssetEditorTabManager::SaveActiveTab()
{
    if (ActiveTabIndex < 0 || ActiveTabIndex >= static_cast<int32>(Tabs.size()) || !Tabs[ActiveTabIndex])
    {
        return false;
    }

    IAssetEditor *Editor = Tabs[ActiveTabIndex]->GetEditor();
    return Editor ? Editor->Save() : false;
}

void FAssetEditorTabManager::Tick(float DeltaTime)
{
    for (const std::unique_ptr<FAssetEditorTab> &Tab : Tabs)
    {
        if (Tab)
        {
            Tab->Tick(DeltaTime);
        }
    }
}

void FAssetEditorTabManager::Render(float DeltaTime, ImGuiID DockspaceId)
{
    for (int32 Index = 0; Index < static_cast<int32>(Tabs.size());)
    {
        FAssetEditorTab *Tab = Tabs[Index].get();
        if (!Tab)
        {
            CloseTab(Index);
            continue;
        }

        bool bOpen = true;
        FString WindowTitle = FString(Tab->GetTitle()) + "###AssetEditorTab" + std::to_string(Index);

        if (DockspaceId != 0)
        {
            ImGui::SetNextWindowDockID(DockspaceId, ImGuiCond_FirstUseEver);
        }

        ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse;
        if (ImGui::Begin(WindowTitle.c_str(), &bOpen, Flags))
        {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            {
                ActiveTabIndex = Index;
            }

            Tab->Render(DeltaTime);
        }
        ImGui::End();

        if (!bOpen)
        {
            CloseTab(Index);
            continue;
        }

        ++Index;
    }
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
    if (ActiveTabIndex < 0 || ActiveTabIndex >= static_cast<int32>(Tabs.size()) || !Tabs[ActiveTabIndex])
    {
        return false;
    }

    IAssetEditor *Editor = Tabs[ActiveTabIndex]->GetEditor();
    return Editor && Editor->IsCapturingInput();
}

IAssetEditor *FAssetEditorTabManager::GetActiveEditor() const
{
    if (ActiveTabIndex < 0 || ActiveTabIndex >= static_cast<int32>(Tabs.size()) || !Tabs[ActiveTabIndex])
    {
        return nullptr;
    }

    return Tabs[ActiveTabIndex]->GetEditor();
}
