#pragma once

#include "Component/Gizmo/GizmoTypes.h"

class FGizmoManager;

// Editor context 단위로 공유되는 transform gizmo tool state.
//
// Level Editor / Asset Editor 모두 viewport는 여러 개일 수 있지만, 사용자가 고른
// transform tool 설정은 보통 해당 editor context 안에서 하나로 공유되어야 한다.
// Viewport별 FGizmoManager는 각자 UGizmoVisualComponent와 picking/drag state를 갖고,
// 이 shared state만 editor context에서 일괄 적용받는다.
struct FEditorGizmoSharedState
{
    EGizmoMode Mode = EGizmoMode::Translate;
    EGizmoSpace Space = EGizmoSpace::Local;

    bool bTranslationSnapEnabled = false;
    bool bRotationSnapEnabled = false;
    bool bScaleSnapEnabled = false;
    float TranslationSnapSize = 10.0f;
    float RotationSnapSizeDegrees = 15.0f;
    float ScaleSnapSize = 0.25f;

    void ApplyTo(FGizmoManager& Manager) const;
};
