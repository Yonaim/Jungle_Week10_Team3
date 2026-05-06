#include "CameraModifier.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_ABSTRACT_CLASS(UCameraModifier, UObject)

UCameraModifier::UCameraModifier()
{

}

void UCameraModifier::AddedToCamera(APlayerCameraManager* InCameraManager) {
	if (InCameraManager) 
		CameraOwner = InCameraManager;
}

bool UCameraModifier::IsFinished() const
{
	return !bPendingDisable && bDisabled;
}

void UCameraModifier::DisableModifier(bool bImmediate) {
	if (bImmediate)
	{
		bDisabled = true;
		bPendingDisable = false;
		return;
	}

	bPendingDisable = true;
}
