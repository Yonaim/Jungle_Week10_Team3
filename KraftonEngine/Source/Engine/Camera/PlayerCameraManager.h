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
	FMinimalViewInfo  POV;
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

	// 매 틱마다 호출
	void UpdateCamera(float DeltaTime);

	void SetOwner(APlayerController* InPlayerController);

	// 카메라 모디파이어 추가 후 중요도에 따라 정렬
	void AddCameraModifier(UCameraModifier* InModifier);

	// 카메라 모디파이어 적용 (매 틱 호출됨)
	void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);

	void StartCameraShake();
	void EndCameraShake();

	void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color);
	void EndCameraFade();

public:
	FViewTarget		ViewTarget;
	FName			CameraStyle;

	// Fade
	bool			bEnableFading = false;
	FLinearColor	FadeColor	  = FLinearColor(0, 0, 0, 1); // Black
	float			FadeAmount    = 0.f;
	FVector2		FadeAlpha;		// FadeAlpha.X = Start Alpha = Initial Opacity
									// FadeAlpha.Y = End Alpha	 = Target Opacity
	float			FadeTime;		
	float			FadeTimeRemaining = 0.f;

private:
	APlayerController* Owner = nullptr;
	TArray<UCameraModifier*> ModifierList;
};