#pragma once
#include "Engine/Core/RayTypes.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Math/Matrix.h"
#include "Math/MathUtils.h"
#include "Math/Vector.h"
#include "Core/EngineTypes.h"
#include "Collision/ConvexVolume.h"

struct FPostProcessSettings
{
	FLinearColor FadeColor  = FLinearColor(0, 0, 0, 1);
};

// Frame snapshot of the current camera state
struct FMinimalViewInfo
{
	FVector  Location = FVector(0, 0, 0);
	FRotator Rotation = FRotator(0, 0, 0);
	float	 FOV = 3.14159265358979f / 3.0f;
	float	 AspectRatio = 16.0f / 9.0f;
	float	 NearZ = 0.1f;
	float	 FarZ = 1000.0f;
	float	 OrthoWidth = 10.0f;
	bool	 bIsOrthogonal = false;

	FPostProcessSettings PostProcessSettings;
};

class UCameraShakeBase;
class UCameraComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UCameraComponent, USceneComponent)

	UCameraComponent() = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void LookAt(const FVector& Target);
	void SetCameraState(const FMinimalViewInfo& NewState);
	const FMinimalViewInfo& GetCameraState() const;

	void SetFOV(float InFOV) { CameraState.FOV = InFOV; }
	void SetOrthoWidth(float InWidth) { CameraState.OrthoWidth = InWidth; }
	void SetOrthographic(bool bOrtho) { CameraState.bIsOrthogonal = bOrtho; }

	void OnResize(int32 Width, int32 Height);

	FMatrix GetViewMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetViewProjectionMatrix() const;
	FConvexVolume GetConvexVolume() const;

	float GetFOV() const { return CameraState.FOV; }
	float GetNearPlane() const { return CameraState.NearZ; }
	float GetFarPlane() const { return CameraState.FarZ; }
	float GetOrthoWidth() const { return CameraState.OrthoWidth; }
	bool IsOrthogonal() const { return CameraState.bIsOrthogonal; }

	FRay DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight);

public://expose in lua 
	void StartCameraShake(float Intensity, float duration); // TODO: Remove this. Let PlayerCameraManager do the job
	void AddHitEffect(float Intensity, float Duration);

	float GetHitEffectIntensity() const { return HitEffectIntensity; }

private:
	void RefreshCameraStateTransform() const;

	mutable FMinimalViewInfo CameraState;


	TArray<UCameraShakeBase*> ActiveShakes;
	FVector AdditiveLocationOffset = FVector::ZeroVector;
	FRotator AdditiveRotationOffset = FRotator::ZeroRotator;

	// Hit Effect
	float HitEffectIntensity = 0.0f;
	float HitEffectDuration = 1.0f;
};
