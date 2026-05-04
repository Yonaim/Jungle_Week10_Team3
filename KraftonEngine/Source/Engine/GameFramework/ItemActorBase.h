#pragma once

#include "GameFramework/AActor.h"

class UBillboardComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UScriptComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

// 아이템 공통 동작을 C++/Lua 양쪽에서 같은 이름으로 다루기 위한 플래그입니다
// C++는 안전한 기본 구조와 충돌 설정을 담당하고, 실제 효과/점수/소멸 로직은 Lua에서 선택적으로 사용합니다
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
	InteractOnClick = 1 << 9,   // 향후 click 상호작용 아이템을 위한 확장 지점입니다.
	InteractOnKey = 1 << 10,    // 향후 key press 상호작용 아이템을 위한 확장 지점입니다.
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

// 범용 아이템 Actor의 C++ 기반 클래스입니다.
// Trigger는 overlap 판정만 담당하고, visual/presentation component는 collision을 끈 상태로 자유롭게 조합합니다.
// 아이템별 개성은 ItemBase.lua를 require/override하는 Lua script에서 구현합니다.
class AItemActorBase : public AActor
{
public:
	DECLARE_CLASS(AItemActorBase, AActor)

	AItemActorBase();

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay() override;
	void Serialize(FArchive& Ar) override;

	UPrimitiveComponent* GetItemTrigger() const;
	UBoxComponent* GetItemTriggerBox() const { return ItemTrigger; }
	UScriptComponent* GetItemScript() const { return ItemScript; }
	const TArray<UPrimitiveComponent*>& GetPresentationComponents() const { return PresentationComponents; }

	void SetItemScript(const FString& ScriptPath);

	UStaticMeshComponent* AddStaticMeshPresentation(const FString& StaticMeshPath = "None");
	UTextRenderComponent* AddTextPresentation(const FString& Text = "Item");
	UBillboardComponent* AddBillboardPresentation(const FString& TexturePath = "None");

	// AddPresentation Helper
	// 등록된 presentation component는 자동으로 trigger에 attach, collision/overlap이 비활성화됩니다.
	template<typename T>
	T* AddPresentationComponent()
	{
		static_assert(std::is_base_of_v<UPrimitiveComponent, T>, "Presentation component must derive from UPrimitiveComponent");
		T* Component = AddComponent<T>();
		RegisterPresentationComponent(Component);
		return Component;
	}

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
	void RegisterPresentationComponent(UPrimitiveComponent* Component);
	void ApplyPresentationDefaults(UPrimitiveComponent* Component);
	void ApplyTriggerDefaults();

	// 실제 overlap 판정을 담당합니다.
	UBoxComponent* ItemTrigger = nullptr;
	// ItemBase.lua, LogItem.lua/CrashDumpItem.lua/EffectItem.lua 같은 item별 script로 교체 가능
	// 상속받은 페이지에서 접근해서 수정 (아이템 공통 속성)
	UScriptComponent* ItemScript = nullptr;
	// StaticMesh/Text/Billboard 등 표시용 component 추적 (충돌X)
	TArray<UPrimitiveComponent*> PresentationComponents;

	FItemInteractionConfig InteractionConfig;

	// default flag값
	uint32 ItemFeatureFlags =
		static_cast<uint32>(EItemFeatureFlags::PickupOnOverlap)
		| static_cast<uint32>(EItemFeatureFlags::ConsumeOnPickup)
		| static_cast<uint32>(EItemFeatureFlags::SingleUse);
};
