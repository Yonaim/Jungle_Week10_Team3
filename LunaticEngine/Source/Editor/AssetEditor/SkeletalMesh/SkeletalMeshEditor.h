#pragma once

#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshDetailsPanel.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshPreviewViewport.h"
#include "AssetEditor/SkeletalMesh/UI/SkeletonTreePanel.h"
#include "Common/UI/EditorPanel.h"

#include <filesystem>
#include <string>

#include "ImGui/imgui.h"

class UObject;
class UEditorEngine;
class FRenderer;
class USkeletalMesh;

/**
 * SkeletalMesh Viewer / Asset Editor.
 *
 * 김연하 담당 범위:
 * - SkeletalMesh 에셋 열기
 * - Viewer 패널 구성
 * - Preview Viewport 표시
 * - Mesh 정보 Details 표시
 * - Reference Pose / Skinned Pose 보기 상태 제공
 *
 * 패널 구성:
 * - Toolbar Panel
 * - Preview Viewport Panel
 * - Skeleton Tree Panel
 * - Details Panel
 *
 * 구현 규칙:
 * - 각 영역은 실제 독립 패널로 렌더링한다.
 * - Level Editor 패널과 같은 FEditorPanel wrapper를 사용해 title/icon/inset 스타일을 통일한다.
 * - Preview Viewport는 FEditorViewportClient 베이스를 재사용한다.
 *
 * 김형도 담당 예정 영역은 SkeletonTreePanel / DetailsPanel / PreviewViewportClient 내부에 placeholder로 남겨둔다.
 * Bone hierarchy, pose edit, bone gizmo, skeleton debug draw는 이 클래스가 직접 완성하지 않는다.
 */
class FSkeletalMeshEditor final : public IAssetEditor
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer) override;
    bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) override;
    void Close() override;
    bool Save() override;

    void Tick(float DeltaTime) override;
    void RenderContent(float DeltaTime) override;

    // SkeletalMeshEditor는 단일 탭 내부 content가 아니라 여러 도킹 패널을 직접 렌더링한다.
    bool UsesExternalPanels() const override { return true; }
    void RenderPanels(float DeltaTime, ImGuiID DockspaceId) override;
    void BuildCustomMenus() override;

    bool IsDirty() const override { return bDirty; }
    bool IsCapturingInput() const override { return bCapturingInput; }
    void CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) override;
    const char *GetEditorName() const override { return "Skeletal Mesh"; }
    const std::filesystem::path &GetAssetPath() const override { return EditingAssetPath; }

  private:
    void RenderPanelsInternal(float DeltaTime, ImGuiID DockspaceId);
    void RenderToolbarPanel(const FEditorPanelDesc &PanelDesc);
    void BuildDefaultDockLayout(ImGuiID DockspaceId);

    std::string MakePanelStableId(const char *PanelName) const;
    FEditorPanelDesc MakePanelDesc(const char *DisplayName, const char *StableName, const char *IconKey,
                                   ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse) const;

  private:
    UEditorEngine        *EditorEngine = nullptr;
    FRenderer            *Renderer = nullptr;
    USkeletalMesh        *EditingAsset = nullptr;
    std::filesystem::path EditingAssetPath;

    // 에디터 전체 공유 상태. 패널들이 이 상태를 같이 보고 갱신한다.
    FSkeletalMeshEditorState State;

    FSkeletalMeshEditorToolbar Toolbar;
    FSkeletalMeshPreviewViewport PreviewViewport;
    FSkeletonTreePanel SkeletonTreePanel;
    FSkeletalMeshDetailsPanel DetailsPanel;

    bool bOpen = false;
    bool bDirty = false;

    // 동일 타입 에디터를 여러 개 열었을 때 ImGui ID 충돌을 피하기 위한 인스턴스 ID.
    uint32 EditorInstanceId = 0;
    ImGuiID BuiltDockspaceId = 0;

    bool bCapturingInput = false;
};
