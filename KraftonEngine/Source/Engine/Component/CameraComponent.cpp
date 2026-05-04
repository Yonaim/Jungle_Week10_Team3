#include "Component/CameraComponent.h"
#include "Object/ObjectFactory.h"
#include "Camera/CameraShake.h"
#include <cmath>

IMPLEMENT_CLASS(UCameraComponent, USceneComponent)
HIDE_FROM_COMPONENT_LIST(UCameraComponent)

FMatrix UCameraComponent::GetViewMatrix() const
{
	UpdateWorldMatrix();

	// Apply additive shake offsets only for view matrix calculation
	FVector FinalLoc = GetWorldLocation() + AdditiveLocationOffset;
	FRotator FinalRot(GetWorldRotation());
	FinalRot.Yaw += AdditiveRotationOffset.Yaw;
	FinalRot.Pitch += AdditiveRotationOffset.Pitch;
	FinalRot.Roll += AdditiveRotationOffset.Roll;

	return FMatrix::MakeViewMatrix(FinalRot.GetRightVector(), FinalRot.GetUpVector(), FinalRot.GetForwardVector(), FinalLoc);
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	if (!CameraState.bIsOrthogonal) {
		return FMatrix::PerspectiveFovLH(CameraState.FOV, CameraState.AspectRatio, CameraState.NearZ, CameraState.FarZ);
	}
	else {
		float HalfW = CameraState.OrthoWidth * 0.5f;
		float HalfH = HalfW / CameraState.AspectRatio;
		return FMatrix::OrthoLH(HalfW * 2.0f, HalfH * 2.0f, CameraState.NearZ, CameraState.FarZ);
	}
}

FMatrix UCameraComponent::GetViewProjectionMatrix() const
{
	return GetViewMatrix() * GetProjectionMatrix();
}

FConvexVolume UCameraComponent::GetConvexVolume() const
{
	FConvexVolume ConvexVolume;
	ConvexVolume.UpdateFromMatrix(GetViewMatrix() * GetProjectionMatrix());
	return ConvexVolume;
}

void UCameraComponent::LookAt(const FVector& Target)
{
	FVector Position = GetWorldLocation();
	FVector Diff = (Target - Position).Normalized();

	constexpr float Rad2Deg = 180.0f / 3.14159265358979f;

	FRotator LookRotation = GetRelativeRotation();
	LookRotation.Pitch = -asinf(Diff.Z) * Rad2Deg;

	if (fabsf(Diff.Z) < 0.999f) {
		LookRotation.Yaw = atan2f(Diff.Y, Diff.X) * Rad2Deg;
	}

	SetRelativeRotation(LookRotation);
}

void UCameraComponent::OnResize(int32 Width, int32 Height)
{
	CameraState.AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
}

void UCameraComponent::SetCameraState(const FCameraState& NewState)
{
	CameraState = NewState;
}

FRay UCameraComponent::DeprojectScreenToWorld(float MouseX, float MouseY, float ScreenWidth, float ScreenHeight) {
	FRay Ray{};
	if (ScreenWidth <= 0.0f || ScreenHeight <= 0.0f)
	{
		Ray.Origin = GetWorldLocation();
		Ray.Direction = GetForwardVector();
		return Ray;
	}

	float NdcX = (2.0f * MouseX) / ScreenWidth - 1.0f;
	float NdcY = 1.0f - (2.0f * MouseY) / ScreenHeight;

	// Reversed-Z: near plane = 1, far plane = 0
	const FVector NdcNear(NdcX, NdcY, 1.0f);
	const FVector NdcFar(NdcX, NdcY, 0.0f);

	const FMatrix InverseViewProjection = (GetViewMatrix() * GetProjectionMatrix()).GetInverse();
	const FVector WorldNear = InverseViewProjection.TransformPositionWithW(NdcNear);
	const FVector WorldFar = InverseViewProjection.TransformPositionWithW(NdcFar);

	FVector Dir = WorldFar - WorldNear;
	const float Length = std::sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y + Dir.Z * Dir.Z);
	Dir = (Length > 1e-4f) ? (Dir / Length) : GetForwardVector();

	if (CameraState.bIsOrthogonal)
	{
		Ray.Origin = WorldNear;
		Ray.Direction = GetForwardVector();
	}
	else
	{
		Ray.Origin = GetWorldLocation();
		Ray.Direction = Dir;
	}

	return Ray;
}

void UCameraComponent::BeginPlay()
{
	USceneComponent::BeginPlay();
	// 카메라 쉐이크/히트 이펙트는 엔진 내부 갱신 로직이라 사용자 bTickEnable과 무관하게 항상 tick
	SetComponentTickEnabled(true);
}

void UCameraComponent::StartCameraShake(float Intensity, float duration)
{
	USinWaveCameraShake* NewShake = UObjectManager::Get().CreateObject<USinWaveCameraShake>(this);
	NewShake->Intensity = Intensity;
	NewShake->Duration = duration;
	ActiveShakes.push_back(NewShake);
}

void UCameraComponent::AddHitEffect(float Intensity, float Duration)
{
	HitEffectIntensity = Intensity;
	HitEffectDuration = (Duration > 0.0f) ? Duration : 1.0f;
}

void UCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	USceneComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Hit Effect Fade out
	if (HitEffectIntensity > 0.0f)
	{
		HitEffectIntensity -= (1.0f / HitEffectDuration) * DeltaTime;
		if (HitEffectIntensity < 0.0f) HitEffectIntensity = 0.0f;
	}

	AdditiveLocationOffset = FVector::ZeroVector;
	AdditiveRotationOffset = FRotator::ZeroRotator;

	for (int32 i = static_cast<int32>(ActiveShakes.size()) - 1; i >= 0; --i)
	{
		UCameraShakeBase* Shake = ActiveShakes[i];
		if (!Shake) continue;

		FVector LocOffset = FVector::ZeroVector;
		FRotator RotOffset = FRotator::ZeroRotator;

		Shake->UpdateShake(DeltaTime, LocOffset, RotOffset);

		AdditiveLocationOffset += LocOffset;
		AdditiveRotationOffset.Pitch += RotOffset.Pitch;
		AdditiveRotationOffset.Yaw += RotOffset.Yaw;
		AdditiveRotationOffset.Roll += RotOffset.Roll;

		if (Shake->IsFinished())
		{
			UObjectManager::Get().DestroyObject(Shake);
			ActiveShakes.erase(ActiveShakes.begin() + i);
		}
	}
}

void UCameraComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "FOV",         EPropertyType::Float, &CameraState.FOV, 0.1f,   3.14f,    0.01f });
	OutProps.push_back({ "Near Z",      EPropertyType::Float, &CameraState.NearZ, 0.01f,  100.0f,   0.01f });
	OutProps.push_back({ "Far Z",       EPropertyType::Float, &CameraState.FarZ, 1.0f,   100000.0f, 10.0f });
	OutProps.push_back({ "Orthographic",EPropertyType::Bool,  &CameraState.bIsOrthogonal});
	OutProps.push_back({ "Ortho Width", EPropertyType::Float, &CameraState.OrthoWidth, 0.1f,   1000.0f,  0.5f });
}
