#pragma once
#include "Component/CameraComponent.h"
#include "Core/CoreTypes.h"
#include "Object/Object.h"

class APlayerCameraManager;

class UCameraModifier : public UObject
{
public:
	DECLARE_CLASS(UCameraModifier, UObject)
	UCameraModifier();

	virtual void AddedToCamera(APlayerCameraManager* Camera)				= 0;

	/**
	 * Directly modifies variables in the owning camera
	 * @param	DeltaTime	Change in time since last update
	 * @param	InOutPOV	Current Point of View, to be updated.
	 * @return	bool		True if should STOP looping the chain, false otherwise
	 */
	 // 원래는 FMinimalViewInfo 라는 이름의 struct를 사용해야 함
	 // TODO: FCameraState 에다 월드 위치 정보 추가
	virtual bool ModifyCamera(float DeltaTime, UCameraComponent& InOutPOV)  = 0;


	virtual void UpdateAlpha(float DeltaTime)								= 0;

	virtual bool IsDisabled() const { return bDisabled; }
	virtual void EnableModifier()	{ bDisabled = true; }
	virtual void ToggleModifier()	{ bDisabled = ~bDisabled; }
	virtual void DisableModifier(bool bImmediate = false);

protected:
	virtual ~UCameraModifier() = default;

public:
	// Unreal Engine도 Public에 넣어둠
	uint8 Priority;

protected:
	APlayerCameraManager* CameraOwner = nullptr;

	float AlphaInTime		= 0.f;;
	float AlphaOutTime		= 0.f;
	float Alpha				= 0.f;
	uint32 bPendingDisable	= false;
	uint32 bDisabled		= false;
};

