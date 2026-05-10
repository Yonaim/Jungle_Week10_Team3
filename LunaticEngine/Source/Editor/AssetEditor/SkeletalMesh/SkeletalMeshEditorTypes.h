#pragma once

#include "Core/CoreTypes.h"

// Skeletal Mesh Editor에서 사용하는 프리뷰 표시 모드.
// 실제 Reference Pose / Skinned Pose 렌더링은 Runtime 담당자가 USkeletalMeshComponent와 연동하면 여기서 선택값만 넘겨주면 된다.
enum class ESkeletalMeshPreviewMode : uint8
{
    ReferencePose = 0,
    SkinnedPose = 1,
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
    bool bShowReferencePose = true;
    bool bEnablePoseEditMode = false;
    bool bShowMeshStatsOverlay = true;
    bool bFramePreviewRequested = false;

    ESkeletalMeshPreviewMode PreviewMode = ESkeletalMeshPreviewMode::ReferencePose;
};
