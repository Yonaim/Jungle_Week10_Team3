#include "Scripting/LuaComponentProxy.h"

#include "Component/ActorComponent.h"
#include "Component/Movement/InterpToMovementComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/Movement/ProjectileMovementComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/SoundComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/UIButtonComponent.h"
#include "Component/UIImageComponent.h"
#include "Component/TextRenderComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "Mesh/ObjManager.h"
#include "Object/Object.h"
#include "Object/UClass.h"
#include "Scripting/LuaActorProxy.h"

#include <algorithm>
#include <cmath>

#ifdef check
#pragma push_macro("check")
#undef check
#define LUA_COMPONENT_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define LUA_COMPONENT_RESTORE_CHECKF_MACRO
#endif

#include <sol/sol.hpp>

#ifdef LUA_COMPONENT_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_COMPONENT_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_COMPONENT_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_COMPONENT_RESTORE_CHECK_MACRO
#endif

namespace
{
	AActor* ResolveAliveActor(AActor* Actor)
	{
		return (Actor && IsAliveObject(Actor)) ? Actor : nullptr;
	}

	UActorComponent* ResolveAliveComponent(UActorComponent* Component)
	{
		if (!Component || !IsAliveObject(Component))
		{
			return nullptr;
		}

		AActor* OwnerActor = ResolveAliveActor(Component->GetOwner());
		if (!OwnerActor)
		{
			return nullptr;
		}

		const TArray<UActorComponent*>& OwnerComponents = OwnerActor->GetComponents();
		const auto It = std::find(OwnerComponents.begin(), OwnerComponents.end(), Component);
		return (It != OwnerComponents.end()) ? Component : nullptr;
	}

	FString StripLuaClassPrefix(const FString& Name)
	{
		if (Name.size() > 1 && Name[0] == 'U')
		{
			return Name.substr(1);
		}

		return Name;
	}
}

bool FLuaComponentProxy::IsValid() const
{
	// Proxy는 Component를 소유하지 않는 약한 참조다.
	return GetComponent() != nullptr;
}

UActorComponent* FLuaComponentProxy::GetComponent() const
{
	// Lua에 전달된 Proxy가 오래 보관된 뒤 호출될 수 있으므로 매번 생존 여부를 다시 확인한다.
	return ResolveAliveComponent(Component);
}

FString FLuaComponentProxy::GetName() const
{
	UActorComponent* TargetComponent = GetComponent();
	return TargetComponent ? TargetComponent->GetFName().ToString() : FString();
}

FString FLuaComponentProxy::GetTypeName() const
{
	UActorComponent* TargetComponent = GetComponent();
	if (!TargetComponent || !TargetComponent->GetClass())
	{
		return FString();
	}

	return StripLuaClassPrefix(TargetComponent->GetClass()->GetName());
}

FLuaActorProxy FLuaComponentProxy::GetOwner() const
{
	FLuaActorProxy OwnerProxy;

	UActorComponent* TargetComponent = GetComponent();
	if (!TargetComponent)
	{
		return OwnerProxy;
	}

	// Owner도 ActorProxy로 감싸서 반환한다.
	OwnerProxy.Actor = ResolveAliveActor(TargetComponent->GetOwner());
	return OwnerProxy;
}

bool FLuaComponentProxy::SetActive(bool bActive)
{
	UActorComponent* TargetComponent = GetComponent();
	if (!TargetComponent)
	{
		return false;
	}

	TargetComponent->SetActive(bActive);
	return true;
}

bool FLuaComponentProxy::IsActive() const
{
	UActorComponent* TargetComponent = GetComponent();
	return TargetComponent ? TargetComponent->IsActive() : false;
}

sol::optional<FVector> FLuaComponentProxy::GetLocation() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetWorldLocation();
}

bool FLuaComponentProxy::SetLocation(const FVector& InLocation)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetWorldLocation(InLocation);
	return true;
}

bool FLuaComponentProxy::SetLocationXYZ(float X, float Y, float Z)
{
	return SetLocation(FVector(X, Y, Z));
}

bool FLuaComponentProxy::AddWorldOffset(const FVector& Delta)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->AddWorldOffset(Delta);
	return true;
}

bool FLuaComponentProxy::AddWorldOffsetXYZ(float X, float Y, float Z)
{
	return AddWorldOffset(FVector(X, Y, Z));
}

sol::optional<FVector> FLuaComponentProxy::GetRotation() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetRelativeRotation().ToVector();
}

bool FLuaComponentProxy::SetRotation(const FVector& InRotation)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetRelativeRotation(InRotation);
	return true;
}

bool FLuaComponentProxy::SetRotationXYZ(float X, float Y, float Z)
{
	return SetRotation(FVector(X, Y, Z));
}

sol::optional<FVector> FLuaComponentProxy::GetScale() const
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return sol::nullopt;
	}

	return SceneComponent->GetRelativeScale();
}

bool FLuaComponentProxy::SetScale(const FVector& InScale)
{
	USceneComponent* SceneComponent = Cast<USceneComponent>(GetComponent());
	if (!SceneComponent)
	{
		return false;
	}

	SceneComponent->SetRelativeScale(InScale);
	return true;
}

bool FLuaComponentProxy::SetScaleXYZ(float X, float Y, float Z)
{
	return SetScale(FVector(X, Y, Z));
}

bool FLuaComponentProxy::SetCollisionEnabled(bool bEnabled)
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent());
	if (!PrimitiveComponent)
	{
		return false;
	}

	PrimitiveComponent->SetCollisionEnabled(bEnabled);
	return true;
}

bool FLuaComponentProxy::SetGenerateOverlapEvents(bool bEnabled)
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent());
	if (!PrimitiveComponent)
	{
		return false;
	}

	PrimitiveComponent->SetGenerateOverlapEvents(bEnabled);
	return true;
}

bool FLuaComponentProxy::IsOverlappingActor(const FLuaActorProxy& OtherActor) const
{
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(GetComponent());
	AActor* TargetActor = OtherActor.GetActor();
	if (!PrimitiveComponent || !TargetActor)
	{
		return false;
	}

	return PrimitiveComponent->IsOverlappingActor(TargetActor);
}

bool FLuaComponentProxy::SetStaticMesh(const FString& MeshPath)
{
	UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(GetComponent());
	if (!StaticMeshComponent || MeshPath.empty() || MeshPath == "None")
	{
		return false;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	// 현재 프로젝트의 StaticMeshComponent와 같은 OBJ 로딩 경로를 그대로 사용한다.
	UStaticMesh* LoadedMesh = FObjManager::LoadObjStaticMesh(MeshPath, Device);
	if (!LoadedMesh)
	{
		return false;
	}

	StaticMeshComponent->SetStaticMesh(LoadedMesh);
	return true;
}

bool FLuaComponentProxy::SetText(const FString& Text)
{
	UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(GetComponent());
	if (!TextComponent)
	{
		return false;
	}

	TextComponent->SetText(Text);
	TextComponent->PostEditProperty("Text");
	return true;
}

sol::optional<FString> FLuaComponentProxy::GetText() const
{
	UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(GetComponent());
	if (!TextComponent)
	{
		return sol::nullopt;
	}

	return TextComponent->GetText();
}

bool FLuaComponentProxy::SetTexture(const FString& TexturePath)
{
	UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent());
	if (!ImageComponent)
	{
		return false;
	}

	return ImageComponent->SetTexturePath(TexturePath);
}

sol::optional<FString> FLuaComponentProxy::GetTexturePath() const
{
	UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent());
	if (!ImageComponent)
	{
		return sol::nullopt;
	}

	return ImageComponent->GetTexturePath();
}

bool FLuaComponentProxy::SetTint(const FVector& TintRGB)
{
	return SetTintRGBA(TintRGB.X, TintRGB.Y, TintRGB.Z, 1.0f);
}

bool FLuaComponentProxy::SetTintRGBA(float R, float G, float B, float A)
{
	UUIImageComponent* ImageComponent = Cast<UUIImageComponent>(GetComponent());
	if (!ImageComponent)
	{
		return false;
	}

	ImageComponent->SetTint(FVector4(R, G, B, A));
	return true;
}

bool FLuaComponentProxy::SetLabel(const FString& Label)
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	if (!ButtonComponent)
	{
		return false;
	}

	ButtonComponent->SetLabel(Label);
	return true;
}

sol::optional<FString> FLuaComponentProxy::GetLabel() const
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	if (!ButtonComponent)
	{
		return sol::nullopt;
	}

	return ButtonComponent->GetLabel();
}

bool FLuaComponentProxy::IsHovered() const
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	return ButtonComponent ? ButtonComponent->IsHovered() : false;
}

bool FLuaComponentProxy::IsPressed() const
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	return ButtonComponent ? ButtonComponent->IsPressed() : false;
}

bool FLuaComponentProxy::WasClicked() const
{
	UIButtonComponent* ButtonComponent = Cast<UIButtonComponent>(GetComponent());
	return ButtonComponent ? ButtonComponent->WasClicked() : false;
}

bool FLuaComponentProxy::SetSoundPath(const FString& SoundPath)
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return false;
	}

	SoundComponent->SetSound(FName(SoundPath));
	return true;
}

sol::optional<FString> FLuaComponentProxy::GetSoundPath() const
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return sol::nullopt;
	}

	return SoundComponent->GetSound().ToString();
}

bool FLuaComponentProxy::SetSoundCategory(const FString& CategoryName)
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return false;
	}

	ESoundCategory Category = ESoundCategory::SFX;
	if (!USoundComponent::TryParseCategory(CategoryName, Category))
	{
		return false;
	}

	SoundComponent->SetCategory(Category);
	return true;
}

sol::optional<FString> FLuaComponentProxy::GetSoundCategory() const
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return sol::nullopt;
	}

	return USoundComponent::CategoryToString(SoundComponent->GetCategory());
}

bool FLuaComponentProxy::SetSoundLooping(bool bLooping)
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	if (!SoundComponent)
	{
		return false;
	}

	SoundComponent->SetLooping(bLooping);
	return true;
}

bool FLuaComponentProxy::IsSoundLooping() const
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->IsLooping() : false;
}

bool FLuaComponentProxy::PlayAudio()
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->Play() : false;
}

bool FLuaComponentProxy::PlayAudioPath(const FString& SoundPath)
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->PlayPath(SoundPath) : false;
}

bool FLuaComponentProxy::StopSound()
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->Stop() : false;
}

bool FLuaComponentProxy::PauseSound()
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->Pause() : false;
}

bool FLuaComponentProxy::ResumeSound()
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->Resume() : false;
}

bool FLuaComponentProxy::IsSoundPlaying() const
{
	USoundComponent* SoundComponent = Cast<USoundComponent>(GetComponent());
	return SoundComponent ? SoundComponent->IsPlaying() : false;
}

bool FLuaComponentProxy::SetSpeed(float Speed)
{
	const float SafeSpeed = (std::max)(0.0f, std::isfinite(Speed) ? Speed : 0.0f);

	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		InterpComponent->SetSpeed(SafeSpeed);
		return true;
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		ProjectileComponent->SetInitialSpeed(SafeSpeed);
		return true;
	}

	return false;
}

sol::optional<float> FLuaComponentProxy::GetSpeed() const
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		return InterpComponent->GetSpeed();
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		return ProjectileComponent->GetInitialSpeed();
	}

	return sol::nullopt;
}

bool FLuaComponentProxy::MoveTo(const FVector& Target)
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		InterpComponent->MoveTo(Target);
		return true;
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		USceneComponent* UpdatedComponent = ProjectileComponent->GetUpdatedComponent();
		if (!UpdatedComponent)
		{
			ProjectileComponent->ResolveUpdatedComponent();
			UpdatedComponent = ProjectileComponent->GetUpdatedComponent();
		}
		if (!UpdatedComponent)
		{
			return false;
		}

		FVector Direction = Target - UpdatedComponent->GetWorldLocation();
		if (Direction.IsNearlyZero())
		{
			ProjectileComponent->StopSimulating();
			return true;
		}

		Direction.Normalize();
		ProjectileComponent->SetVelocity(Direction);
		return true;
	}

	return false;
}

bool FLuaComponentProxy::MoveToXYZ(float X, float Y, float Z)
{
	return MoveTo(FVector(X, Y, Z));
}

bool FLuaComponentProxy::MoveBy(const FVector& Delta)
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		InterpComponent->MoveBy(Delta);
		return true;
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		if (Delta.IsNearlyZero())
		{
			ProjectileComponent->StopSimulating();
			return true;
		}

		FVector Direction = Delta;
		Direction.Normalize();
		ProjectileComponent->SetVelocity(Direction);
		return true;
	}

	return false;
}

bool FLuaComponentProxy::MoveByXYZ(float X, float Y, float Z)
{
	return MoveBy(FVector(X, Y, Z));
}

bool FLuaComponentProxy::StopMove()
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		InterpComponent->StopMove();
		return true;
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		ProjectileComponent->StopSimulating();
		return true;
	}

	return false;
}

bool FLuaComponentProxy::IsMoveDone() const
{
	if (UInterpToMovementComponent* InterpComponent = Cast<UInterpToMovementComponent>(GetComponent()))
	{
		return InterpComponent->IsMoveDone();
	}

	if (UProjectileMovementComponent* ProjectileComponent = Cast<UProjectileMovementComponent>(GetComponent()))
	{
		return ProjectileComponent->GetVelocity().IsNearlyZero();
	}

	return false;
}
