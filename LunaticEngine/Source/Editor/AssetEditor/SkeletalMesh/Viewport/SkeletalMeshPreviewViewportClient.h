#pragma once

#include "Common/Viewport/EditorViewportClient.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "Render/Scene/FScene.h"

class UCameraComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class FPrimitiveSceneProxy;

struct ImVec2;

/**
 * SkeletalMesh Editor 전용 Preview Viewport Client.
 *
 * 설계 의도:
 * - LevelEditorViewportClient를 그대로 재사용하지 않는다.
 * - FEditorViewportClient의 공통 viewport rect / image 출력 경로는 공유한다.
 * - 실제 렌더 대상은 Level World가 아니라 Preview 전용 FScene이다.
 *
 * 김연하 담당 범위:
 * - Preview Viewport 패널과 렌더 요청 연결
 * - Preview 전용 Camera / Scene / SkeletalMeshComponent 관리
 * - 입력은 Asset Preview용 orbit / pan / zoom 중심으로 처리
 *
 * 남윤지 담당 영역 연결:
 * - USkeletalMeshComponent가 CPU Skinning 결과를 만들고,
 * - FSkeletalMeshProxy가 그 결과를 Dynamic VB로 업로드한다.
 */
class FSkeletalMeshPreviewViewportClient final : public FEditorViewportClient
{
  public:
    FSkeletalMeshPreviewViewportClient() = default;
    ~FSkeletalMeshPreviewViewportClient() override;

    void Init(FWindowsWindow *InWindow) override;
    void Shutdown() override;
    void Tick(float DeltaTime) override;

    void SetPreviewMesh(USkeletalMesh *InMesh);
    USkeletalMesh *GetPreviewMesh() const { return PreviewMesh; }

    void SetEditorState(FSkeletalMeshEditorState *InState) { State = InState; }

    void RenderViewportImage(bool bIsActiveViewport) override;
    const char *GetViewportTooltipBarText() const override;
    bool BuildRenderRequest(FEditorViewportRenderRequest &OutRequest) override;

    USkeletalMeshComponent *GetPreviewComponent() const { return PreviewComponent; }
    UCameraComponent *GetPreviewCamera() const { return PreviewCamera; }

  private:
    void EnsurePreviewObjects();
    void ReleasePreviewObjects();
    void RebuildPreviewProxy();

    void ResetPreviewCamera();
    void FramePreviewMesh();
    void TickViewportInput(float DeltaTime);
    void RenderFallbackOverlay();

	// 임시 Bone 회전 테스트용 함수
	void RenderPoseEditDebugControls();
	void RenderSkeletonDebugOverlay();
	bool ProjectWorldToViewport(const FVector& WorldPos, ImVec2& OutScreen) const;

  private:
    USkeletalMesh *PreviewMesh = nullptr;
    FSkeletalMeshEditorState *State = nullptr;

    // Asset Preview 전용 렌더 Scene. LevelEditor World / Selection / PIE와 분리한다.
    FScene PreviewScene;
    USkeletalMeshComponent *PreviewComponent = nullptr;
    UCameraComponent *PreviewCamera = nullptr;
    FPrimitiveSceneProxy *PreviewProxy = nullptr;

    FViewportRenderOptions RenderOptions;

    FVector OrbitTarget = FVector::ZeroVector;
    float OrbitDistance = 6.0f;
    float OrbitYaw = 180.0f;
    float OrbitPitch = -10.0f;
    bool bPreviewObjectsInitialized = false;
};
