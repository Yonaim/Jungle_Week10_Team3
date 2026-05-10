#pragma once

#include "Common/Viewport/EditorViewportClient.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"

class USkeletalMesh;

/**
 * SkeletalMesh Editor 전용 Preview Viewport Client.
 *
 * LevelEditorViewportClient와 같은 FEditorViewportClient 베이스를 사용한다.
 * 즉, Preview Viewport도 Level Viewport와 같은 viewport 공통 경로를 탄다.
 *
 * 김연하 담당 범위:
 * - Preview Viewport 패널에서 공통 Viewport 기능 재사용
 * - Mesh / PreviewMode / Bone 선택 상태를 렌더 영역에 전달할 준비
 * - 실제 렌더 연결 전까지 fallback overlay 표시
 *
 * 남윤지 담당 영역 Placeholder:
 * - USkeletalMeshComponent / USkinnedMeshComponent가 준비되면 이 Client가 Preview Scene 또는 Preview Component를 소유한다.
 * - Reference Pose / Skinned Pose 결과를 Viewport RenderTarget에 렌더링한다.
 *
 * 김형도 담당 영역 Placeholder:
 * - Skeleton Debug Draw, Bone Gizmo, Pose Edit Mode 입력 처리는 이 Client 또는 별도 Gizmo Controller에 연결한다.
 */
class FSkeletalMeshPreviewViewportClient final : public FEditorViewportClient
{
  public:
    void Tick(float DeltaTime) override;

    void SetPreviewMesh(USkeletalMesh *InMesh) { PreviewMesh = InMesh; }
    USkeletalMesh *GetPreviewMesh() const { return PreviewMesh; }

    void SetEditorState(FSkeletalMeshEditorState *InState) { State = InState; }

    void RenderViewportImage(bool bIsActiveViewport) override;

  private:
    void RenderFallbackOverlay();

  private:
    USkeletalMesh *PreviewMesh = nullptr;
    FSkeletalMeshEditorState *State = nullptr;
};
