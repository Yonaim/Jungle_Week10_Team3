#include "PCH/LunaticPCH.h"
#include "Common/Gizmo/EditorGizmoSharedState.h"

#include "Common/Gizmo/GizmoManager.h"

void FEditorGizmoSharedState::ApplyTo(FGizmoManager& Manager) const
{
    const EGizmoSpace EffectiveSpace = Mode == EGizmoMode::Scale ? EGizmoSpace::Local : Space;

    Manager.SetMode(Mode);
    Manager.SetSpace(EffectiveSpace);
    Manager.SetSnapSettings(bTranslationSnapEnabled, TranslationSnapSize,
                            bRotationSnapEnabled, RotationSnapSizeDegrees,
                            bScaleSnapEnabled, ScaleSnapSize);
    Manager.SyncVisualFromTarget();
}
