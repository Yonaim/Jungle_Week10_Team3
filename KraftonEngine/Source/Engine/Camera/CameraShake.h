#pragma once
#include "Object/Object.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Object/ObjectFactory.h"

class UCameraShakeBase : public UObject
{
public:
	DECLARE_CLASS(UCameraShakeBase, UObject)

	UCameraShakeBase() = default;

	float Duration = 0.5f;
	float Intensity = 1.0f;
	float ElapsedTime = 0.0f;

	// Update shake state and return local offsets
	virtual void UpdateShake(float DeltaTime, FVector& OutLoc, FRotator& OutRot) = 0;
	bool IsFinished() const { return ElapsedTime >= Duration; }
};

class USinWaveCameraShake : public UCameraShakeBase
{
public:
	DECLARE_CLASS(USinWaveCameraShake, UCameraShakeBase)
	USinWaveCameraShake() = default;

	void UpdateShake(float DeltaTime, FVector& OutLoc, FRotator& OutRot) override;
};
