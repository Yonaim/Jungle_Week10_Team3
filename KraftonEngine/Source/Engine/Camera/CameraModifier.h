#pragma once
#include "Component/CameraComponent.h"
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
	 * Directly modifies variables in the owning camera
	 * @param	DeltaTime	Change in time since last update
	 * @param	InOutPOV	Current Point of View, to be updated.
	 * @return	bool		True if should STOP looping the chain, false otherwise
	 */
	// 원래는 FMinimalViewInfo 라는 이름의 struct를 사용해야 함
	// TODO: FCameraState 에다 월드 위치 정보 추가
	virtual bool ModifyCamera(float DeltaTime, UCameraComponent* InOutPOV);


	virtual void UpdateAlpha(float DeltaTime);

	virtual bool IsDisabled() const { return bDisabled; }
	virtual void EnableModifier();
	virtual void ToggleModifier();
	virtual void DisableModifier(bool bImmediate = false);

	float GetAlpha() const { return Alpha; }
	bool IsPendingDisable() const { return bPendingDisable != 0; }
	void SetAlphaInTime(float InAlphaInTime) { AlphaInTime = InAlphaInTime > 0.0f ? InAlphaInTime : 0.0f; }
	void SetAlphaOutTime(float InAlphaOutTime) { AlphaOutTime = InAlphaOutTime > 0.0f ? InAlphaOutTime : 0.0f; }
	float GetAlphaInTime() const { return AlphaInTime; }
	float GetAlphaOutTime() const { return AlphaOutTime; }


	// Curve Accessors
	UCurveFloat* GetTransitionCurveX() const { return TransitionCurveX != nullptr ? TransitionCurveX : nullptr; }
	virtual void SetTransitionCurveX(UCurveFloat* InCurve) { TransitionCurveX = InCurve; }
	UCurveFloat* GetTransitionCurveY() const { return TransitionCurveY != nullptr ? TransitionCurveY : nullptr; }
	virtual void SetTransitionCurveY(UCurveFloat* InCurve) { TransitionCurveY = InCurve; }
	UCurveFloat* GetTransitionCurveZ() const { return TransitionCurveZ != nullptr ? TransitionCurveZ : nullptr; }
	virtual void SetTransitionCurveZ(UCurveFloat* InCurve) { TransitionCurveZ = InCurve; }

	UCurveFloat* GetRotationCurveX() const { return RotationCurveX != nullptr ? RotationCurveX : nullptr; }
	virtual void SetRotationCurveX(UCurveFloat* InCurve) { RotationCurveX = InCurve; }
	UCurveFloat* GetRotationCurveY() const { return RotationCurveY != nullptr ? RotationCurveY : nullptr; }
	virtual void SetRotationCurveY(UCurveFloat* InCurve) { RotationCurveY = InCurve; }
	UCurveFloat* GetRotationCurveZ() const { return RotationCurveZ != nullptr ? RotationCurveZ : nullptr; }
	virtual void SetRotationCurveZ(UCurveFloat* InCurve) { RotationCurveZ = InCurve; }

protected:
	virtual ~UCameraModifier() = default;

public:
	// Unreal Engine도 Public에 넣어둠
	uint8 Priority  = 0;
	float Intensity = 1.f;

protected:
	APlayerCameraManager* CameraOwner = nullptr;

	UCurveFloat* TransitionCurveX = nullptr;
	UCurveFloat* TransitionCurveY = nullptr;
	UCurveFloat* TransitionCurveZ = nullptr;
	UCurveFloat* RotationCurveX   = nullptr;
	UCurveFloat* RotationCurveY   = nullptr;
	UCurveFloat* RotationCurveZ   = nullptr;

	// Time it takes for Alpha to go from 0.0 to 1.0
	float AlphaInTime		= 0.f;

	// Time it takes for Alpha to go from 1.0 to 0.0
	float AlphaOutTime		= 0.f;

	float Alpha				= 0.f;
	uint32 bPendingDisable	= false;
	uint32 bDisabled		= false;
};

