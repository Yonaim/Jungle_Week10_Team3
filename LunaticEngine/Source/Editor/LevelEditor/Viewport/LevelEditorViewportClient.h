#pragma once

#include "Common/Viewport/EditorViewportClient.h"
#include "Common/Viewport/EditorViewportCamera.h"
#include "Core/CollisionTypes.h"
#include "Core/CoreTypes.h"
#include "Core/RayTypes.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputAction.h"
#include "Input/InputMappingContext.h"
#include "Math/Rotator.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"

#include "imgui.h"

class UWorld;
class UGizmoComponent;
class ULightComponentBase;
class AActor;
class FLevelEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class USceneComponent;
class FOverlayStatSystem;

// Level Editor ?꾩슜 Viewport Client.
// 移대찓??議곗옉, Actor picking, Gizmo 議곗옉, View mode, PIE shortcut ?깆쓣 ?대떦?쒕떎.
class FLevelEditorViewportClient : public FEditorViewportClient
{
  public:
    FLevelEditorViewportClient();
    ~FLevelEditorViewportClient() override;

    void Init(FWindowsWindow *InWindow) override;
    void Shutdown() override;
    void Tick(float DeltaTime) override;

    UWorld *GetWorld() const;

    void             SetOverlayStatSystem(FOverlayStatSystem *InOverlayStatSystem) { OverlayStatSystem = InOverlayStatSystem; }
    void             SetGizmo(UGizmoComponent *InGizmo) { Gizmo = InGizmo; GizmoManager.SetVisualComponent(InGizmo); }
    void             SetSettings(const FLevelEditorSettings *InSettings) { Settings = InSettings; }
    void             SetSelectionManager(FSelectionManager *InSelectionManager) { SelectionManager = InSelectionManager; }
    UGizmoComponent *GetGizmo() { return Gizmo; }

    FViewportRenderOptions       &GetRenderOptions() { return RenderOptions; }
    const FViewportRenderOptions &GetRenderOptions() const { return RenderOptions; }

    void SetViewportType(ELevelViewportType NewType);
    void SetViewportSize(float InWidth, float InHeight);

    void              CreateCamera();
    void              DestroyCamera();
    void              ResetCamera();
    FEditorViewportCamera *GetCamera() { return FEditorViewportClient::GetCamera(); }
    const FEditorViewportCamera *GetCamera() const { return FEditorViewportClient::GetCamera(); }
    bool              FocusActor(AActor *Actor);

    void                 SetLightViewOverride(ULightComponentBase *Light);
    void                 ClearLightViewOverride();
    bool                 IsViewingFromLight() const { return LightViewOverride != nullptr; }
    ULightComponentBase *GetLightViewOverride() const { return LightViewOverride; }

    int32 GetPointLightFaceIndex() const { return PointLightFaceIndex; }
    void  SetPointLightFaceIndex(int32 Index) { PointLightFaceIndex = (Index < 0) ? 0 : (Index > 5) ? 5 : Index; }

    void UpdateLayoutRect() override;
    void RenderViewportImage(bool bIsActiveViewport) override;
    const char *GetViewportTooltipBarText() const override;
    bool BuildRenderRequest(FEditorViewportRenderRequest &OutRequest) override;

  private:
    void SetupInput();

    void OnEditorMove(const FInputActionValue &Value);
    void OnEditorRotate(const FInputActionValue &Value);
    void OnEditorPan(const FInputActionValue &Value);
    void OnEditorZoom(const FInputActionValue &Value);
    void OnEditorOrbit(const FInputActionValue &Value);

    void OnEditorFocus(const FInputActionValue &Value);
    void OnEditorDelete(const FInputActionValue &Value);
    void OnEditorDuplicate(const FInputActionValue &Value);
    void OnEditorToggleGizmoMode(const FInputActionValue &Value);
    void OnEditorToggleCoordSystem(const FInputActionValue &Value);
    void OnEditorEscape(const FInputActionValue &Value);
    void OnEditorTogglePIE(const FInputActionValue &Value);

    void  TickEditorShortcuts();
    void  TickInput(float DeltaTime);
    void  TickInteraction(float DeltaTime);
    void  SyncGizmoTargetFromSelection();
    void  HandleDragStart(const FRay &Ray);
    void  DrawUIScreenTranslateGizmo();
    bool  HasUIScreenTranslateGizmo() const;
    int32 HitTestUIScreenTranslateGizmo(const ImVec2 &MousePos) const;
    bool  BeginUIScreenTranslateDrag(const ImVec2 &MousePos);
    void  UpdateUIScreenTranslateDrag(const ImVec2 &MousePos);
    void  EndUIScreenTranslateDrag(bool bCommitChange);
    void  SyncCameraSmoothingTarget();
    void  ApplySmoothedCameraLocation(float DeltaTime);

  private:
    FOverlayStatSystem    *OverlayStatSystem = nullptr;
    UGizmoComponent       *Gizmo = nullptr;
    const FLevelEditorSettings *Settings = nullptr;
    FSelectionManager     *SelectionManager = nullptr;
    USceneComponent        *CurrentGizmoTargetComponent = nullptr;
    FViewportRenderOptions RenderOptions;
    ULightComponentBase   *LightViewOverride = nullptr;
    int32                  PointLightFaceIndex = 0;

    bool    bIsMarqueeSelecting = false;
    FVector MarqueeStartPos;
    FVector MarqueeCurrentPos;

    bool        bIsFocusAnimating = false;
    FVector     FocusStartLoc;
    FRotator    FocusStartRot;
    FVector     FocusEndLoc;
    FRotator    FocusEndRot;
    float       FocusAnimTimer = 0.0f;
    const float FocusAnimDuration = 0.5f;

    FVector     TargetLocation;
    bool        bTargetLocationInitialized = false;
    FVector     LastAppliedCameraLocation;
    bool        bLastAppliedCameraLocationInitialized = false;
    const float SmoothLocationSpeed = 10.0f;

    FEnhancedInputManager EnhancedInputManager;
    FInputMappingContext *EditorMappingContext = nullptr;

    FInputAction *ActionEditorMove = nullptr;
    FInputAction *ActionEditorRotate = nullptr;
    FInputAction *ActionEditorPan = nullptr;
    FInputAction *ActionEditorZoom = nullptr;
    FInputAction *ActionEditorOrbit = nullptr;

    FInputAction *ActionEditorFocus = nullptr;
    FInputAction *ActionEditorDelete = nullptr;
    FInputAction *ActionEditorDuplicate = nullptr;
    FInputAction *ActionEditorToggleGizmoMode = nullptr;
    FInputAction *ActionEditorToggleCoordSystem = nullptr;
    FInputAction *ActionEditorEscape = nullptr;
    FInputAction *ActionEditorTogglePIE = nullptr;
    FInputAction *ActionEditorDecreaseSnap = nullptr;
    FInputAction *ActionEditorIncreaseSnap = nullptr;
    FInputAction *ActionEditorVertexSnap = nullptr;
    FInputAction *ActionEditorSnapToFloor = nullptr;
    FInputAction *ActionEditorSetBookmark = nullptr;
    FInputAction *ActionEditorJumpToBookmark = nullptr;
    FInputAction *ActionEditorSetViewportPerspective = nullptr;
    FInputAction *ActionEditorSetViewportTop = nullptr;
    FInputAction *ActionEditorSetViewportFront = nullptr;
    FInputAction *ActionEditorSetViewportRight = nullptr;
    FInputAction *ActionEditorToggleGridSnap = nullptr;
    FInputAction *ActionEditorToggleRotationSnap = nullptr;
    FInputAction *ActionEditorToggleScaleSnap = nullptr;

    FVector EditorMoveAccumulator = FVector::ZeroVector;
    FVector EditorRotateAccumulator = FVector::ZeroVector;
    FVector EditorPanAccumulator = FVector::ZeroVector;
    float   EditorZoomAccumulator = 0.0f;
    int32   HoveredUIScreenGizmoAxis = 0;
    int32   ActiveUIScreenGizmoAxis = 0;
    bool    bDraggingUIScreenGizmo = false;
    ImVec2  LastUIScreenGizmoMousePos = ImVec2(0.0f, 0.0f);
};
