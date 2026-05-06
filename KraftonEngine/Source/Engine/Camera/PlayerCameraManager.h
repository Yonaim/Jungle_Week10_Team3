#pragma once
#include "GameFramework/AActor.h"
#include "CameraModifier.h"
#include "Core/EngineTypes.h"
#include "Core/CollisionEventTypes.h"  

class USciptComponent;
class APawnActor;
class APlayerController;

struct FViewTarget {
public:
	void SetNewTarget(AActor* InTarget);
	APawnActor* GetTargetPawn() const;
	bool Equal(const FViewTarget& OtherTarget) const;
	void CheckViewTarget(APlayerController* OwningController);

public:
	AActor*			  Target = nullptr;

	// 원래는 FMinimalViewInfo 라는 이름의 struct를 사용해야 함
	// TODO: FCameraState 에다 월드 위치 정보 추가
	UCameraComponent* POV	 = nullptr;
};


class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)

	// Fade in
	void BeginPlay() override;

	// Fade Out
	void EndPlay() override;

	// PlayerManager는 모디파이어 리스트를 상시 순회함
	void Tick(float DeltaTime) override;

	void SetOwner(APlayerController* InPlayerController) { Owner = InPlayerController; }

	// 카메라 모디파이어 추가 후 중요도에 따라 정렬
	void AddCameraModifier(UCameraModifier* InModifier);

	// 카메라 모디파이어 적용 (매 틱 호출됨)
	void ApplyCameraModifiers(float DeltaTime, UCameraComponent* InOutPOV);

	//void StartCameraShake();
	//void EndCameraShake();

	//void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color);
	//void EndCameraFade();

public:
	FViewTarget		ViewTarget;
	FName			CameraStyle;
	FLinearColor	FadeColor;
	float			FadeAmount;
	FVector2		FadeAlpha;
	float			FadeTime;
	float			FadeTimeRemaining;

private:
	APlayerController* Owner = nullptr;
	TArray<UCameraModifier*> ModifierList;
};