#pragma once

#include "Common/Viewport/EditorViewportClient.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "Common/Gizmo/GizmoManager.h"
#include "Common/Viewport/EditorViewportCamera.h"
#include "Common/Viewport/PreviewScene.h"

#include <memory>

class USkeletalMesh;
class USkeletalMeshComponent;
class FPrimitiveSceneProxy;
class FSkeletalMeshSelectionManager;
class FSkeletalMeshPreviewPoseController;
struct FRay;

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
 * - Preview 전용 Scene / SkeletalMeshComponent 관리
 * - 카메라 입력은 공통 FViewportCameraController를 사용하고, Level Editor와 같은 자유비행 방식을 따른다.
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
    void SetSelectionManager(FSkeletalMeshSelectionManager *InSelectionManager) { SelectionManager = InSelectionManager; }
    void SetPoseController(std::shared_ptr<FSkeletalMeshPreviewPoseController> InPoseController);

    void ActivateForTabSwitch(FSkeletalMeshEditorState* InState, FSkeletalMeshSelectionManager* InSelectionManager);
    void DeactivateForTabSwitch();

    void RenderViewportImage(bool bIsActiveViewport) override;
    const char *GetViewportTooltipBarText() const override;
    bool BuildRenderRequest(FEditorViewportRenderRequest &OutRequest) override;

    USkeletalMeshComponent *GetPreviewComponent() const { return PreviewComponent; }
    FEditorViewportCamera *GetPreviewCamera() { return &ViewCamera; }
    const FEditorViewportCamera *GetPreviewCamera() const { return &ViewCamera; }

  private:
    void EnsurePreviewObjects();
    void ReleasePreviewObjects();
    void RebuildPreviewProxy();

    void ResetPreviewCamera();
    void FramePreviewMesh();
    void TickViewportInput(float DeltaTime);
    void TickGizmoInteraction();
    void CycleGizmoModeFromShortcut();
    void RenderFallbackOverlay();
    void SubmitSkeletonDebugDraw();
    int32 HitTestBoneSelection(const FRay& Ray) const;
    void ApplyBoneViewportSelection(int32 BoneIndex);
    void ApplyEditorStateToViewport();
    void SyncRenderOptionsFromState();
    void ApplyViewportTypeToCamera();
    void SyncGizmoTargetFromSelection();

  private:
    USkeletalMesh *PreviewMesh = nullptr;
    FSkeletalMeshEditorState *State = nullptr;
    FSkeletalMeshSelectionManager *SelectionManager = nullptr;

    // Asset Preview 전용 렌더 Scene. LevelEditor World / Selection / PIE와 분리한다.
    FPreviewScene PreviewScene;
    USkeletalMeshComponent *PreviewComponent = nullptr;
    FPrimitiveSceneProxy *PreviewProxy = nullptr;

    FViewportRenderOptions RenderOptions;
    int32 GizmoTargetBoneIndex = -1;
    std::shared_ptr<FSkeletalMeshPreviewPoseController> PoseController;

    ESkeletalMeshPreviewViewportType LastAppliedViewportType = ESkeletalMeshPreviewViewportType::Perspective;
    bool bHasAppliedViewportType = false;

    // Focus/ortho 전환용 preview 기준점이다. 입력 이동은 orbit target이 아니라 ViewCamera 자체에 적용한다.
    FVector OrbitTarget = FVector::ZeroVector;
    float OrbitDistance = 6.0f;
    float OrbitYaw = 180.0f;
    float OrbitPitch = -10.0f;
    bool bPreviewObjectsInitialized = false;
};
