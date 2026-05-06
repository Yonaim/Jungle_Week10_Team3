#pragma once
#include "GameFramework/AActor.h"
#include "CameraModifier.h"
#include "Core/EngineTypes.h"

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
	void AddCameraModifier(UCameraModifier* InModifier);
	void ApplyCameraModifiers(float DeltaTime, UCameraComponent* InOutPOV);

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