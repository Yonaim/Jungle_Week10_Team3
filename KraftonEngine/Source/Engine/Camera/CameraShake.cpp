#include "Camera/CameraShake.h"
#include <cmath>

IMPLEMENT_ABSTRACT_CLASS(UCameraShakeBase, UObject)
IMPLEMENT_CLASS(USinWaveCameraShake, UCameraShakeBase)

void USinWaveCameraShake::UpdateShake(float DeltaTime, FVector& OutLoc, FRotator& OutRot)
{
	ElapsedTime += DeltaTime;
	float Alpha = 1.0f - (ElapsedTime / Duration);
	if (Alpha < 0.0f) Alpha = 0.0f;

	// Simple sinusoidal movement on Y and Z axis
	float Wave = sinf(ElapsedTime * 25.0f) * Intensity * Alpha;
	OutLoc = FVector(0.0f, Wave, Wave * 0.5f);
	
	// Subtle rotation shake on Roll and Yaw
	OutRot.Roll = Wave * 0.2f;
	OutRot.Yaw = Wave * 0.1f;
}
