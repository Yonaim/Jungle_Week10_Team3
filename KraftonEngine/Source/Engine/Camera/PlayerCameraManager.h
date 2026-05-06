#pragma once
#include "GameFramework/AActor.h"
#include "CameraModifier.h"
#include "Core/EngineTypes.h"

class APawnActor;
class APlayerController;
class UCameraComponent;
struct FViewTarget {
public:
	void SetNewTarget(AActor* InTarget);
	APawnActor* GetTargetPawn() const;
	bool Equal(const FViewTarget& OtherTarget) const;
	void CheckViewTarget(APlayerController* OwningController);

public:
	AActor*			  Target = nullptr;
	FMinimalViewInfo POV;
	// 원래는 FMinimalViewInfo 라는 이름의 struct를 사용해야 함
};

struct FCameraCacheEntry
{
	float TimeStamp=0.0f;
	FMinimalViewInfo POV;
};

class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)
	void AddCameraModifier(UCameraModifier* InModifier);

	void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);
	void UpdateCamera(float deltaTime);
	void SortModifiersByPriority();
	void RemoveFinishedModifiers();
	// Find CameraComponent to actor
public:
	FViewTarget		ViewTarget;
	FName			CameraStyle;
	FLinearColor	FadeColor;
	float			FadeAmount;
	FVector2		FadeAlpha;
	float			FadeTime;
	float			FadeTimeRemaining;
	FCameraCacheEntry CameraCache;
	FCameraCacheEntry LastFrameCameraCache;
private:
	UCameraComponent* FindCameraComponent(AActor* Target);
	FMinimalViewInfo BuildFallbackCameraView(AActor* Target) const;
private:
	APlayerController* Owner = nullptr;
	TArray<UCameraModifier*> ModifierList;
};