#pragma once

#include "AssetEditor/Tabs/AssetEditorTab.h"
#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"

#include <filesystem>
#include <memory>

class IAssetEditor;

class FAssetEditorTabManager
{
  public:
    bool OpenTab(std::unique_ptr<IAssetEditor> Editor);
    bool ActivateTabByAssetPath(const std::filesystem::path &AssetPath);

    void CloseTab(int32 TabIndex);
    void CloseActiveTab();
    bool SaveActiveTab();

    void Tick(float DeltaTime);
    void Render(float DeltaTime, ImGuiID DockspaceId = 0);

    bool HasOpenTabs() const;
    int32 GetTabCount() const;
    bool IsCapturingInput() const;
    IAssetEditor *GetActiveEditor() const;

  private:
    TArray<std::unique_ptr<FAssetEditorTab>> Tabs;
    int32 ActiveTabIndex = -1;
};
