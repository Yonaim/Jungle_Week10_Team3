#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class UActorComponent;
struct FLuaActorProxy;

namespace sol
{
	template <typename T>
	class optional;
}

struct FLuaComponentProxy
{
	UActorComponent* Component = nullptr;

	bool IsValid() const;
	UActorComponent* GetComponent() const;

	FString GetName() const;
	FString GetTypeName() const;
	FLuaActorProxy GetOwner() const;

	bool SetActive(bool bActive);
	bool IsActive() const;

	sol::optional<FVector> GetLocation() const;
	bool SetLocation(const FVector& InLocation);
	bool SetLocationXYZ(float X, float Y, float Z);
	bool AddWorldOffset(const FVector& Delta);
	bool AddWorldOffsetXYZ(float X, float Y, float Z);
	sol::optional<FVector> GetRotation() const;
	bool SetRotation(const FVector& InRotation);
	bool SetRotationXYZ(float X, float Y, float Z);
	sol::optional<FVector> GetScale() const;
	bool SetScale(const FVector& InScale);
	bool SetScaleXYZ(float X, float Y, float Z);

	bool SetCollisionEnabled(bool bEnabled);
	bool SetGenerateOverlapEvents(bool bEnabled);
	bool IsOverlappingActor(const FLuaActorProxy& OtherActor) const;

	bool SetStaticMesh(const FString& MeshPath);

	bool SetText(const FString& Text);
	sol::optional<FString> GetText() const;

	bool SetTexture(const FString& TexturePath);
	sol::optional<FString> GetTexturePath() const;
	bool SetTint(const FVector& TintRGB);
	bool SetTintRGBA(float R, float G, float B, float A);
	bool SetLabel(const FString& Label);
	sol::optional<FString> GetLabel() const;
	bool IsHovered() const;
	bool IsPressed() const;
	bool WasClicked() const;
	bool SetSoundPath(const FString& SoundPath);
	sol::optional<FString> GetSoundPath() const;
	bool SetSoundCategory(const FString& CategoryName);
	sol::optional<FString> GetSoundCategory() const;
	bool SetSoundLooping(bool bLooping);
	bool IsSoundLooping() const;
	bool PlayAudio();
	bool PlayAudioPath(const FString& SoundPath);
	bool StopSound();
	bool PauseSound();
	bool ResumeSound();
	bool IsSoundPlaying() const;

	bool SetSpeed(float Speed);
	sol::optional<float> GetSpeed() const;
	bool MoveTo(const FVector& Target);
	bool MoveToXYZ(float X, float Y, float Z);
	bool MoveBy(const FVector& Delta);
	bool MoveByXYZ(float X, float Y, float Z);
	bool StopMove();
	bool IsMoveDone() const;
};
