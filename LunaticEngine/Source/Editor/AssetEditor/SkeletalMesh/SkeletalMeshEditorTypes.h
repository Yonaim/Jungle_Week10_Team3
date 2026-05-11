#pragma once

#include "Core/CoreTypes.h"
#include "Component/Gizmo/GizmoTypes.h"

// Skeletal Mesh Editor에서 사용하는 프리뷰 표시 모드.
// 실제 Reference Pose / Skinned Pose 렌더링은 Runtime 담당자가 USkeletalMeshComponent와 연동하면 여기서 선택값만 넘겨주면 된다.
enum class ESkeletalMeshPreviewMode : uint8
{
    ReferencePose = 0,
    SkinnedPose = 1,
};

// Skeletal Mesh Preview Viewport의 카메라 방향 선택값.
// Level Editor toolbar와 같은 상단 toolbar를 쓰기 위해 UI 상태로 보관한다.
enum class ESkeletalMeshPreviewViewportType : uint8
{
    Perspective = 0,
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back,
    FreeOrtho,
};

// Skeletal Mesh Preview Viewport의 렌더 표시 모드.
enum class ESkeletalMeshPreviewViewMode : uint8
{
    Lit = 0,
    Unlit,
    LitGouraud,
    LitLambert,
    Wireframe,
    SceneDepth,
    WorldNormal,
    LightCulling,
};

enum class ESkeletalMeshPreviewLayout : uint8
{
    OnePane = 0,
    TwoPanesHorizontal,
    TwoPanesVertical,
    FourPanes2x2,
};

// Skeletal Mesh Viewer / Asset Editor의 현재 편집 상태.
//
// 김연하 담당 범위:
// - Preview 모드, LOD 선택, Section/Material 표시 상태 등 Viewer UI 상태를 관리한다.
//
// 김형도 담당 예정 영역:
// - Bone hierarchy / transform / pose edit / gizmo 상태가 확정되면 이 구조체를 확장한다.
struct FSkeletalMeshEditorState
{
    int32 SelectedBoneIndex = -1;
    int32 CurrentLODIndex = 0;
    int32 SelectedSectionIndex = -1;
    int32 SelectedMaterialSlotIndex = -1;

    bool bShowBones = true;
    bool bShowGrid = true;
    bool bShowReferencePose = true;
    bool bEnablePoseEditMode = false;

    // Level Editor viewport toolbar의 Show Flag / Snap / Camera 항목과 동일한 UI 상태.
    bool bShowPrimitives = true;
    bool bShowSkeletalMesh = true;
    bool bShowBillboardText = true;
    bool bShowWorldAxis = true;
    bool bShowGizmo = true;
    bool bShowSceneBVH = false;
    bool bShowOctree = false;
    bool bShowWorldBound = false;
    bool bShowLightVisualization = false;

    bool bEnableTranslationSnap = false;
    bool bEnableRotationSnap = false;
    bool bEnableScaleSnap = false;
    float TranslationSnapSize = 10.0f;
    float RotationSnapSize = 15.0f;
    float ScaleSnapSize = 0.25f;

    float CameraSpeed = 5.0f;
    float CameraFOV = 60.0f;
    float CameraOrthoWidth = 10.0f;

    float GridSpacing = 1.0f;
    int32 GridHalfLineCount = 100;
    float GridLineThickness = 1.0f;
    float GridMajorLineThickness = 1.5f;
    int32 GridMajorLineInterval = 10;
    float GridMinorIntensity = 0.35f;
    float GridMajorIntensity = 0.70f;
    float AxisThickness = 2.0f;
    float AxisIntensity = 1.0f;
    float DebugLineThickness = 1.0f;
    float BillboardIconScale = 1.0f;
    EGizmoMode GizmoMode = EGizmoMode::Translate;
    EGizmoSpace GizmoSpace = EGizmoSpace::Local;
    bool bShowMeshStatsOverlay = true;
    bool bFramePreviewRequested = false;

    ESkeletalMeshPreviewMode PreviewMode = ESkeletalMeshPreviewMode::ReferencePose;
    ESkeletalMeshPreviewViewportType PreviewViewportType = ESkeletalMeshPreviewViewportType::Perspective;
    ESkeletalMeshPreviewViewMode PreviewViewMode = ESkeletalMeshPreviewViewMode::Lit;
    ESkeletalMeshPreviewLayout PreviewLayout = ESkeletalMeshPreviewLayout::OnePane;
};
