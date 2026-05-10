#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"

#include "ImGui/imgui.h"

class USkeletalMesh;

/**
 * SkeletalMesh Editor의 왼쪽 Skeleton Tree 패널.
 *
 * 김연하 담당 범위:
 * - Skeleton/Bone 목록을 Viewer UI로 표시한다.
 * - Bone 선택 상태를 FSkeletalMeshEditorState에 기록한다.
 * - Bone 검색 / 선택 해제 같은 Viewer 편의 기능을 제공한다.
 *
 * 김형도 담당 예정 영역:
 * - 실제 USkeleton / Bone hierarchy API 연동
 * - Parent/Child 구조 기반 TreeNode 표시
 * - Bone 선택 후 Transform Inspector / Gizmo / Pose 갱신 연결
 */
class FSkeletonTreePanel
{
  public:
    void Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, const char *WindowName, ImGuiID DockspaceId);

  private:
    char SearchBuffer[128] = "";
};
