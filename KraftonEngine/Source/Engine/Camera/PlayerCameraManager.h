#pragma once
#include "GameFramework/AActor.h"
#include "CameraModifier.h"
#include "Core/EngineTypes.h"
#include "Core/CollisionEventTypes.h"

class USciptComponent;
class APawnActor;
class APlayerController;
class UCameraComponent;

struct FViewTarget
{
public:
	void SetNewTarget(AActor* InTarget);
	APawnActor* GetTargetPawn() const;
	bool Equal(const FViewTarget& OtherTarget) const;
	void CheckViewTarget(APlayerController* OwningController);

public:
	AActor* Target = nullptr;
	FMinimalViewInfo POV;
};

struct FCameraCacheEntry
{
	float TimeStamp = 0.0f;
	FMinimalViewInfo POV;
};

class APlayerCameraManager : public AActor
{
public:
	DECLARE_CLASS(APlayerCameraManager, AActor)

	void BeginPlay() override;
	void EndPlay() override;
	void Tick(float DeltaTime) override;

	void SetOwner(APlayerController* InPlayerController) { Owner = InPlayerController; }

	void AddCameraModifier(UCameraModifier* InModifier);
	void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);
	void UpdateCamera(float DeltaTime);
	void SortModifiersByPriority();
	void RemoveFinishedModifiers();

public:
	FViewTarget ViewTarget;
	FName CameraStyle;
	FLinearColor FadeColor;
	float FadeAmount;
	FVector2 FadeAlpha;
	float FadeTime;
	float FadeTimeRemaining;
	FCameraCacheEntry CameraCache;
	FCameraCacheEntry LastFrameCameraCache;

private:
	UCameraComponent* FindCameraComponent(AActor* Target);
	FMinimalViewInfo BuildFallbackCameraView(AActor* Target) const;

private:
	APlayerController* Owner = nullptr;
	TArray<UCameraModifier*> ModifierList;
};
