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
	virtual bool ModifyCamera(float DeltaTime, UCameraComponent& InOutPOV);


	virtual void UpdateAlpha(float DeltaTime);

	virtual bool IsDisabled() const { return bDisabled; }
	virtual void EnableModifier()	{ bDisabled = true; }
	virtual void ToggleModifier()	{ bDisabled = ~bDisabled; }
	virtual void DisableModifier(bool bImmediate = false);

	UCurveFloat* GetCurve() const { return CameraCurve != nullptr ? CameraCurve : nullptr; }
	virtual void SetCurve(UCurveFloat* InCurve) { if (InCurve) CameraCurve = InCurve; }

protected:
	virtual ~UCameraModifier() = default;

public:
	// Unreal Engine도 Public에 넣어둠
	uint8 Priority;

protected:
	APlayerCameraManager* CameraOwner = nullptr;
	UCurveFloat*		  CameraCurve = nullptr;

	// Time it takes for Alpha to go from 0.0 to 1.0
	float AlphaInTime		= 0.f;

	// Time it takes for Alpha to go from 1.0 to 0.0
	float AlphaOutTime		= 0.f;

	float Alpha				= 0.f;
	uint32 bPendingDisable	= false;
	uint32 bDisabled		= false;
};

