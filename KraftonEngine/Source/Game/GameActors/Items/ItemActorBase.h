#pragma once

#include "GameFramework/AActor.h"

class UBillboardComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UScriptComponent;

// Item 동작 flag -> Lua 쪽에도 동시에 존재. 같이 수정해줘야함.
enum class EItemFeatureFlags : uint32
{
	None = 0,
	PickupOnOverlap = 1 << 0,   // Trigger overlap이 발생하면 자동으로 pickup/interact를 시도합니다.
	ConsumeOnPickup = 1 << 1,   // pickup 성공 후 item actor를 제거하는 기본 동작입니다.
	ScoreReward = 1 << 2,       // GameManager.AddScore()로 점수 보상을 지급합니다.
	GameplayEffect = 1 << 3,    // SpeedBoost, Shield 같은 효과 적용을 위한 확장 지점입니다.
	SingleUse = 1 << 4,         // 중복 overlap으로 같은 아이템이 여러 번 발동되는 것을 막습니다.
	AutoRespawn = 1 << 5,       // 향후 respawn item을 위한 예약 플래그입니다.
	FloatingMotion = 1 << 6,    // 향후 떠다니는 연출을 위한 예약 플래그입니다.
	RotatingMotion = 1 << 7,    // 향후 회전 연출을 위한 예약 플래그입니다.
	DebugLog = 1 << 8,          // Lua item script에서 디버그 로그를 켭니다.
};

// 아이템별로 반복되는 값을 한곳에 묶어 둡니다.
struct FItemInteractionConfig
{
	int32 ScoreValue = 0;
	FString RequiredInteractorTag = "Player";
	FString EffectName;
	float EffectDuration = 0.0f;
	float RespawnDelay = 0.0f;
	float Cooldown = 0.0f;
	bool bStartsEnabled = true;
	bool bDestroyOnPickup = true;
};

// Item Actor 기반 클래스
// Trigger: overlap / 아이템 표시: Billboard component
// Item 별 동작 흐름은 Lua script에서 구현
class AItemActorBase : public AActor
{
public:
	DECLARE_CLASS(AItemActorBase, AActor)

	AItemActorBase();

	// Life Cycle
	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay() override;
	void Serialize(FArchive& Ar) override;

	// Item Getter
	UPrimitiveComponent* GetItemTrigger() const;
	UBoxComponent* GetItemTriggerBox() const { return ItemTrigger; }
	UScriptComponent* GetItemScript() const { return ItemScript; }
	UBillboardComponent* GetItemImage() const { return ItemImage; }

	void SetItemScript(const FString& ScriptPath);

	UBillboardComponent* AddBillboardPresentation(const FString& TexturePath = "None");

	// Item Flag 관련
	bool HasFeature(EItemFeatureFlags Feature) const;
	void SetFeatureEnabled(EItemFeatureFlags Feature, bool bEnabled);
	void AddFeature(EItemFeatureFlags Feature);
	void RemoveFeature(EItemFeatureFlags Feature);
	uint32 GetFeatureFlags() const { return ItemFeatureFlags; }
	void SetFeatureFlags(uint32 InFlags) { ItemFeatureFlags = InFlags; }

	FItemInteractionConfig& GetInteractionConfig() { return InteractionConfig; }
	const FItemInteractionConfig& GetInteractionConfig() const { return InteractionConfig; }

	void SetTriggerEnabled(bool bEnabled);
	bool IsTriggerEnabled() const;

protected:
	void ApplyBillboardDefaults();
	void ApplyTriggerDefaults();

	
	UBoxComponent* ItemTrigger = nullptr;
	UScriptComponent* ItemScript = nullptr;
	UBillboardComponent* ItemImage = nullptr;

	FItemInteractionConfig InteractionConfig;

	// default flag값
	uint32 ItemFeatureFlags =
		static_cast<uint32>(EItemFeatureFlags::PickupOnOverlap)
		| static_cast<uint32>(EItemFeatureFlags::ConsumeOnPickup)
		| static_cast<uint32>(EItemFeatureFlags::SingleUse);
};
