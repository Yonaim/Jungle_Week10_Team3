#pragma once
#include "CameraModifier.h"
#include "CameraShake.h"

class UCameraModifier_CameraShake : public UCameraModifier
{
public:
	DECLARE_CLASS(UCameraModifier_CameraShake, UCameraModifier)

	void AddedToCamera(APlayerCameraManager* Camera) override;
	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;
	bool IsFinished() const override;

	void SetCameraShake(UCameraShakeBase* InShake);
	void AddCameraShake(UCameraShakeBase* InShake);
	UCameraShakeBase* GetCameraShake() const;
	const TArray<UCameraShakeBase*>& GetActiveShakes() const { return ActiveShakes; }
	void RemoveFinishedShakes();
	void ClearCameraShakes();

protected:
	~UCameraModifier_CameraShake() override;

private:
	TArray<UCameraShakeBase*> ActiveShakes;
};
