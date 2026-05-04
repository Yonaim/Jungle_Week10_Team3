#include "GameFramework/ItemActorBase.h"

#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/Shape/BoxComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Mesh/ObjManager.h"
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

	// 기본 script만 붙여도 overlap pickup 흐름을 탈 수 있게 합니다.
	// item별 동작은 SetItemScript("Scripts/Game/Items/CoinItem.lua")처럼 교체해서 확장합니다.
	ItemScript = AddComponent<UScriptComponent>();
	ItemScript->SetScriptPath("Scripts/Game/Items/ItemBase.lua");
}

void AItemActorBase::BeginPlay()
{
	// 저장/복제/스크립트 수정 과정에서 trigger 설정이 바뀌었을 수 있으므로 BeginPlay 직전에 한 번 더 보정합니다.
	ApplyTriggerDefaults();
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

UStaticMeshComponent* AItemActorBase::AddStaticMeshPresentation(const FString& StaticMeshPath)
{
	// StaticMesh는 표시 전용입니다. 실제 pickup 판정은 ItemTrigger가 담당합니다.
	UStaticMeshComponent* Component = AddPresentationComponent<UStaticMeshComponent>();
	if (Component && !StaticMeshPath.empty() && StaticMeshPath != "None" && GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		Component->SetStaticMesh(FObjManager::LoadObjStaticMesh(StaticMeshPath, Device));
	}
	return Component;
}

UTextRenderComponent* AItemActorBase::AddTextPresentation(const FString& Text)
{
	// 텍스트만 있는 아이템도 trigger 기반으로 동작할 수 있도록 presentation helper로 분리
	UTextRenderComponent* Component = AddPresentationComponent<UTextRenderComponent>();
	if (Component)
	{
		Component->SetText(Text);
	}
	return Component;
}

UBillboardComponent* AItemActorBase::AddBillboardPresentation(const FString& TexturePath)
{
	// Billboard sprite 아이템도 static mesh와 같은 방식으로 추가합니다.
	UBillboardComponent* Component = AddPresentationComponent<UBillboardComponent>();
	if (Component && !TexturePath.empty() && TexturePath != "None" && GEngine)
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		if (UTexture2D* Texture = UTexture2D::LoadFromFile(TexturePath, Device))
		{
			Component->SetTexture(Texture);
		}
	}
	return Component;
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

void AItemActorBase::RegisterPresentationComponent(UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// presentation component가 overlap 후보가 되면 아이템 pickup이 중복/오동작 가능성 존재
	// 등록 시점에 collision을 끄고, trigger 아래에 붙여 transform만 따라가게 진행
	ApplyPresentationDefaults(Component);
	PresentationComponents.push_back(Component);
	if (ItemTrigger && Component != ItemTrigger)
	{
		Component->AttachToComponent(ItemTrigger);
	}
}

void AItemActorBase::ApplyPresentationDefaults(UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// visual/presentation component는 절대 item pickup 판정에 참여하지 않습니다.
	Component->SetCollisionEnabled(false);
	Component->SetGenerateOverlapEvents(false);
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
