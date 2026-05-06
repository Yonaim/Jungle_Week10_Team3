#include "CameraModifier.h"
#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(UCameraModifier, UObject)

UCameraModifier::UCameraModifier() {}

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

	float TOffsetX = TransitionCurveX != nullptr ? TransitionCurveX->Evaluate(Alpha) : 0;
	float TOffsetY = TransitionCurveY != nullptr ? TransitionCurveY->Evaluate(Alpha) : 0;
	float TOffsetZ = TransitionCurveZ != nullptr ? TransitionCurveZ->Evaluate(Alpha) : 0;
	InOutPOV->AddWorldOffset(FVector(TOffsetX, TOffsetY, TOffsetZ) * TransitionIntensity);
	
	float ROffsetX = RotationCurveX != nullptr ? RotationCurveX->Evaluate(Alpha) : 0;
	float ROffsetY = RotationCurveY != nullptr ? RotationCurveY->Evaluate(Alpha) : 0;
	float ROffsetZ = RotationCurveZ != nullptr ? RotationCurveZ->Evaluate(Alpha) : 0;
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
