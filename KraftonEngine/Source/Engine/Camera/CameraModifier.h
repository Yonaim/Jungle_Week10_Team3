#pragma once
#include "Camera/MinimalViewInfo.h"
#include "CameraShakePattern.h"
#include "Core/CoreTypes.h"
#include "Math/CurveFloat.h"
#include "Object/Object.h"

class APlayerCameraManager;

class UCameraModifier : public UObject
{
public:
	DECLARE_CLASS(UCameraModifier, UObject)
	UCameraModifier();

	virtual void AddedToCamera(APlayerCameraManager* Camera);

	/**
	 * Directly modifies variables in the owning camera.
	 * @param DeltaTime Change in time since last update.
	 * @param InOutPOV Current point of view, to be updated.
	 * @return True if modifier processing should stop, false otherwise.
	 */
	virtual bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV);

	virtual void UpdateAlpha(float DeltaTime);

	virtual bool IsDisabled() const { return bDisabled; }
	virtual bool IsFinished() const;
	virtual void EnableModifier();
	virtual void ToggleModifier();
	virtual void DisableModifier(bool bImmediate = false);

	float GetAlpha() const { return Alpha; }
	bool IsPendingDisable() const { return bPendingDisable != 0; }
	void SetAlphaInTime(float InAlphaInTime) { AlphaInTime = InAlphaInTime > 0.0f ? InAlphaInTime : 0.0f; }
	void SetAlphaOutTime(float InAlphaOutTime) { AlphaOutTime = InAlphaOutTime > 0.0f ? InAlphaOutTime : 0.0f; }
	float GetAlphaInTime() const { return AlphaInTime; }
	float GetAlphaOutTime() const { return AlphaOutTime; }

	void SetCameraShakePattern(UCameraShakePattern* InShakePattern) { ShakePattern = InShakePattern; }
	UCameraShakePattern* GetCameraShakePattern() const { return ShakePattern; }

protected:
	virtual ~UCameraModifier();

public:
	uint8 Priority = 0;
	float TransitionIntensity = 1.0f;
	float RotationIntensity = 1.0f;

protected:
	APlayerCameraManager* CameraOwner = nullptr;
	UCameraShakePattern* ShakePattern = nullptr;

	float AlphaInTime = 0.0f;
	float AlphaOutTime = 0.0f;
	float Alpha = 0.0f;
	uint32 bPendingDisable = false;
	uint32 bDisabled = false;
};
