#include "Common/Gizmo/EditorGizmoSharedState.h"

#include "Common/Gizmo/GizmoManager.h"

void FEditorGizmoSharedState::ApplyTo(FGizmoManager& Manager) const
{
    Manager.SetMode(Mode);
    Manager.SetSpace(Space);
    Manager.SetSnapSettings(bTranslationSnapEnabled, TranslationSnapSize,
                            bRotationSnapEnabled, RotationSnapSizeDegrees,
                            bScaleSnapEnabled, ScaleSnapSize);
    Manager.SyncVisualFromTarget();
}
