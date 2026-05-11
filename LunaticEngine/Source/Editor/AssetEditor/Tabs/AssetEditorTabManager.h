#pragma once

#include "AssetEditor/Tabs/AssetEditorTab.h"
#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"

#include <filesystem>
#include <memory>

class IAssetEditor;
class FEditorViewportClient;

/**
 * 열린 Asset Editor 문서들을 관리하는 관리자.
 *
 * 현재 구현에서는 실제 ImGui TabBar만 의미하지 않는다.
 * UsesExternalPanels()가 true인 에디터는 하나의 탭이 아니라 여러 개의 도킹 패널을 렌더링할 수 있다.
 * 예: SkeletalMeshEditor는 Toolbar / Preview Viewport / Skeleton Tree / Details 패널을 따로 연다.
 *
 * 역할:
 * - 열린 에디터 목록 관리
 * - 같은 에셋 중복 열기 방지 및 기존 패널 활성화
 * - 활성 에디터 저장/닫기
 * - Asset Editor 패널 렌더링
 */
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
    FEditorViewportClient *GetActiveViewportClient() const;
    void CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const;

  private:
    TArray<std::unique_ptr<FAssetEditorTab>> Tabs;
    int32 ActiveTabIndex = -1;
};
