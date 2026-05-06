#include "CameraModifier.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier, UObject)

UCameraModifier::UCameraModifier() {}
UCameraModifier::~UCameraModifier() 
{
	if (ShakePattern) {
		UObjectManager::Get().DestroyObject(ShakePattern);
		ShakePattern = nullptr;
	}	
}

void UCameraModifier::AddedToCamera(APlayerCameraManager* InCameraManager)
{
	CameraOwner = InCameraManager;
}

void UCameraModifier::EnableModifier()
{
	bDisabled = false;
	bPendingDisable = false;
	if (Alpha <= 0.0f)
	{
		Alpha = (AlphaInTime <= 0.0f) ? 1.0f : 0.0f;
	}
}

void UCameraModifier::ToggleModifier()
{
	if (bDisabled || bPendingDisable)
	{
		EnableModifier();
	}
	else
	{
		DisableModifier();
	}
}

void UCameraModifier::DisableModifier(bool bImmediate)
{
	if (bImmediate)
	{
		bDisabled = true;
		bPendingDisable = false;
		Alpha = 0.0f;
		return;
	}

	bPendingDisable = true;
}

bool UCameraModifier::ModifyCamera(float DeltaTime, UCameraComponent* InOutPOV)
{
	//float Wave = Intensity * CameraCurve->Evaluate(Alpha);
	//InOutPOV->AddWorldOffset(FVector(0.0f, Wave, Wave * 0.5f));
	//InOutPOV->AddLocalRotation(FRotator(Wave * 0.05f, Wave * 0.1f, Wave * 0.2f));
	//return false;

	if (!ShakePattern) return false;

	float TOffsetX = ShakePattern->EvalTransitionX(Alpha);
	float TOffsetY = ShakePattern->EvalTransitionY(Alpha);
	float TOffsetZ = ShakePattern->EvalTransitionZ(Alpha);
	InOutPOV->AddWorldOffset(FVector(TOffsetX, TOffsetY, TOffsetZ) * TransitionIntensity);
	
	float ROffsetX = ShakePattern->EvalRotationX(Alpha);
	float ROffsetY = ShakePattern->EvalRotationY(Alpha);
	float ROffsetZ = ShakePattern->EvalRotationZ(Alpha);
	InOutPOV->AddLocalRotation(FRotator(ROffsetX, ROffsetY, ROffsetZ) * RotationIntensity);
	return false;
}

void UCameraModifier::UpdateAlpha(float DeltaTime)
{
	if (bDisabled)
	{
		return;
	}

	if (bPendingDisable)
	{
		if (AlphaOutTime <= 0.0f)
		{
			Alpha = 0.0f;
			bPendingDisable = false;
			bDisabled = true;
			return;
		}

		Alpha -= DeltaTime / AlphaOutTime;
		if (Alpha <= 0.0f)
		{
			Alpha = 0.0f;
			bPendingDisable = false;
			bDisabled = true;
		}
		return;
	}

	if (AlphaInTime <= 0.0f)
	{
		Alpha = 1.0f;
		return;
	}

	Alpha += DeltaTime / AlphaInTime;
	if (Alpha > 1.0f)
	{
		Alpha = 1.0f;
	}
}
