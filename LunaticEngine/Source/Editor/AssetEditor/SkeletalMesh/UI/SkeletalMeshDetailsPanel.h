#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"

#include <filesystem>

#include "ImGui/imgui.h"

class USkeletalMesh;

/**
 * SkeletalMesh Editor의 오른쪽 Details 패널.
 *
 * 김연하 담당 범위:
 * - Mesh 기본 정보 표시
 * - LOD / Section / Material 정보 표시를 위한 자리 제공
 * - 현재 선택된 Bone index / Preview mode 상태 표시
 *
 * 김형도 담당 예정 영역:
 * - Bone Transform Inspector
 * - Local / World Transform 표시
 * - Pose Edit Mode에서의 값 편집 UI
 */
class FSkeletalMeshDetailsPanel
{
  public:
    void Render(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State,
                const char *WindowName, ImGuiID DockspaceId);

  private:
    void RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State);
    void RenderLODSectionMaterialInfo(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State);
    void RenderViewerActions(FSkeletalMeshEditorState &State);
    void RenderRuntimePlaceholder();
    void RenderBoneEditingPlaceholder(FSkeletalMeshEditorState &State);
};
