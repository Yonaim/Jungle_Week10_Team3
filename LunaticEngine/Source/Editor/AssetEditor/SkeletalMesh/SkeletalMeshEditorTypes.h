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
// 4번 담당자가 Bone 선택, Pose Edit, Bone Gizmo를 붙일 때 이 상태를 확장하면 된다.
struct FSkeletalMeshEditorState
{
    int32 SelectedBoneIndex = -1;
    int32 CurrentLODIndex = 0;

    bool bShowBones = true;
    bool bShowReferencePose = true;
    bool bEnablePoseEditMode = false;

    ESkeletalMeshPreviewMode PreviewMode = ESkeletalMeshPreviewMode::ReferencePose;
};
