#include "CameraModifier.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier, UObject)

UCameraModifier::UCameraModifier() {}

void UCameraModifier::AddedToCamera(APlayerCameraManager* InCameraManager) {
	if (!InCameraManager) CameraOwner = InCameraManager;
}

void UCameraModifier::DisableModifier(bool bImmediate) {
	if (bImmediate) bDisabled = true; return;
	bPendingDisable = true;
}

bool UCameraModifier::ModifyCamera(float DeltaTime, UCameraComponent& InOutPOV) {
	float Dx = CameraCurve != nullptr ? CameraCurve->Evaluate(Alpha) : 0;


	UpdateAlpha(DeltaTime);
}

void UCameraModifier::UpdateAlpha(float DeltaTime) {
	Alpha += DeltaTime;
	if (Alpha >= 1.f) {
		Alpha = 0.f;
		if (bPendingDisable) {
			bPendingDisable = false;
			bDisabled = true;
		}
	}
}
