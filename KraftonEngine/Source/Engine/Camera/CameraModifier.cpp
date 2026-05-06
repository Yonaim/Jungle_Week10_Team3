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
	float Dx = 0;
	if (CameraCurve) {
		Dx = CameraCurve->Evaluate(Alpha);
	}


	UpdateAlpha(DeltaTime);
}

void UCameraModifier::UpdateAlpha(float DeltaTime) {
	float Delta = bPendingDisable ? DeltaTime / AlphaOutTime * -1 : DeltaTime / AlphaInTime;
	Alpha += Delta;

	if (Alpha < 0.f || Alpha >= 1.0f) {
		Alpha = 0.f;
		if (bPendingDisable) {
			bPendingDisable = false;
			bDisabled = true;
		}
	}
}
