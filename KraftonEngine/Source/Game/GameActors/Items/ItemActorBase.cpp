#include "ItemActorBase.h"

#include "Component/BillboardComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

IMPLEMENT_CLASS(AItemActorBase, AActor)

AItemActorBase::AItemActorBase()
{
	// Trigger를 root로 두면 Actor transform과 overlap bounds가 같은 기준으로 움직입니다.
	// visual component는 이 trigger 아래에 attach되고, collision은 기본적으로 꺼집니다.
	ItemTrigger = AddComponent<UBoxComponent>();
	ItemTrigger->SetCanDeleteFromDetails(false);
	ItemTrigger->SetBoxExtent(FVector(1.0f, 1.0f, 1.0f));
	ApplyTriggerDefaults();
	SetRootComponent(ItemTrigger);

	ItemImage = AddComponent<UBillboardComponent>();
	ApplyBillboardDefaults();

	// 기본 script만 붙여도 overlap pickup 흐름을 탈 수 있게 합니다.
	// item별 동작은 SetItemScript("Scripts/Game/Items/LogItem.lua")처럼 교체해서 확장합니다.
	ItemScript = AddComponent<UScriptComponent>();
	ItemScript->SetScriptPath("Scripts/Game/Items/ItemBase.lua");
}

void AItemActorBase::BeginPlay()
{
	// 저장/복제/스크립트 수정 과정에서 설정이 바뀌었을 수 있으므로 BeginPlay 직전에 한 번 더 보정합니다.
	ApplyTriggerDefaults();
	ApplyBillboardDefaults();
	Super::BeginPlay();
}

void AItemActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AItemActorBase::EndPlay()
{
	Super::EndPlay();
}

void AItemActorBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	// C++ config는 future-proof 저장 데이터입니다.
	// 현재 gameplay 튜닝은 Lua property가 담당하지만, scene 저장/로드 시 기본값이 사라지지 않게 보존합니다.
	Ar << ItemFeatureFlags;
	Ar << InteractionConfig.ScoreValue;
	Ar << InteractionConfig.RequiredInteractorTag;
	Ar << InteractionConfig.EffectName;
	Ar << InteractionConfig.EffectDuration;
	Ar << InteractionConfig.RespawnDelay;
	Ar << InteractionConfig.Cooldown;
	Ar << InteractionConfig.bStartsEnabled;
	Ar << InteractionConfig.bDestroyOnPickup;
}

UPrimitiveComponent* AItemActorBase::GetItemTrigger() const
{
	return ItemTrigger;
}

void AItemActorBase::SetItemScript(const FString& ScriptPath)
{
	if (ItemScript)
	{
		ItemScript->SetScriptPath(ScriptPath);
	}
}

UBillboardComponent* AItemActorBase::AddBillboardPresentation(const FString& TexturePath)
{
	if (!ItemImage)
	{
		ItemImage = AddComponent<UBillboardComponent>();
	}

	ApplyBillboardDefaults();

	if (ItemImage && !TexturePath.empty() && TexturePath != "None" && GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (UTexture2D* Texture = UTexture2D::LoadFromFile(TexturePath, Device))
		{
			ItemImage->SetTexture(Texture);
		}
	}
	return ItemImage;
}

bool AItemActorBase::HasFeature(EItemFeatureFlags Feature) const
{
	return (ItemFeatureFlags & static_cast<uint32>(Feature)) != 0;
}

void AItemActorBase::SetFeatureEnabled(EItemFeatureFlags Feature, bool bEnabled)
{
	if (bEnabled)
	{
		AddFeature(Feature);
	}
	else
	{
		RemoveFeature(Feature);
	}
}

void AItemActorBase::AddFeature(EItemFeatureFlags Feature)
{
	ItemFeatureFlags |= static_cast<uint32>(Feature);
}

void AItemActorBase::RemoveFeature(EItemFeatureFlags Feature)
{
	ItemFeatureFlags &= ~static_cast<uint32>(Feature);
}

void AItemActorBase::SetTriggerEnabled(bool bEnabled)
{
	if (ItemTrigger)
	{
		ItemTrigger->SetCollisionEnabled(bEnabled);
	}
}

bool AItemActorBase::IsTriggerEnabled() const
{
	return ItemTrigger && ItemTrigger->IsCollisionEnabled();
}

void AItemActorBase::ApplyBillboardDefaults()
{
	if (!ItemImage)
	{
		return;
	}

	ItemImage->SetCollisionEnabled(false);
	ItemImage->SetGenerateOverlapEvents(false);
	if (ItemTrigger)
	{
		ItemImage->AttachToComponent(ItemTrigger);
	}
}

void AItemActorBase::ApplyTriggerDefaults()
{
	if (!ItemTrigger)
	{
		return;
	}

	// Trigger는 query overlap 전용입니다. 
	// block/hit 처리는 player/obstacle 같은 gameplay collider에 맡깁니다.
	ItemTrigger->SetCollisionEnabled(InteractionConfig.bStartsEnabled);
	ItemTrigger->SetGenerateOverlapEvents(true);
}
