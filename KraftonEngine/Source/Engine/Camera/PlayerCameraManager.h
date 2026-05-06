#pragma once
#include "GameFramework/AActor.h"
#include "CameraModifier.h"
#include "Core/EngineTypes.h"
#include "Core/CollisionEventTypes.h"

#include <filesystem>

class USciptComponent;
class APawnActor;
class APlayerController;
class UCameraComponent;
class UCameraModifier_CameraShake;

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

	void SetOwner(APlayerController* InPlayerController);

	// Function : Add camera modifier to manager and sort by priority
	// input : InModifier
	// InModifier : modifier instance that will edit final camera POV
	void AddCameraModifier(UCameraModifier* InModifier);

	// Function : Create and play a Lua camera modifier through PlayerCameraManager
	// input : ScriptPath, Params
	// ScriptPath : Lua modifier script path under Scripts
	// Params : numeric parameters passed to Lua modifier Begin(params)
	void PlayCameraModifier(const FString& ScriptPath, const TMap<FString, float>& Params = {});

	// Function : Apply active camera modifiers to the current POV
	// input : DeltaTime, InOutPOV
	// DeltaTime : frame delta time
	// InOutPOV : camera POV modified by active modifiers in priority order
	void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);

	// Function : Update view target POV and camera cache for this frame
	// input : DeltaTime
	// DeltaTime : frame delta time used by CalcCamera and modifiers
	void UpdateCamera(float DeltaTime);

	void SortModifiersByPriority();
	void RemoveFinishedModifiers();

	// Function : Enable camera shake modifier owned by PlayerCameraManager
	// input : none
	// CameraShakeModifier : modifier that owns active camera shake instances
	void StartCameraShake();

	// Function : Disable camera shake modifier owned by PlayerCameraManager
	// input : none
	// CameraShakeModifier : modifier that owns active camera shake instances
	void EndCameraShake();
	void StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color);
	void EndCameraFade();
	void LoadCameraModifierStackAsset(const std::filesystem::path& AssetPath);

	const FMinimalViewInfo& GetCameraCachePOV() const { return CameraCache.POV; }
	bool HasValidCameraCachePOV() const { return bHasValidCameraCachePOV; }

public:
	FViewTarget ViewTarget;
	FName CameraStyle;

	bool bEnableFading = false;
	FLinearColor FadeColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	float FadeAmount = 0.0f;
	FVector2 FadeAlpha;
	float FadeTime = 0.0f;
	float FadeTimeRemaining = 0.0f;

	FCameraCacheEntry CameraCache;
	FCameraCacheEntry LastFrameCameraCache;

private:
	UCameraComponent* FindCameraComponent(AActor* Target);
	FMinimalViewInfo BuildFallbackCameraView(AActor* Target) const;
	UCameraModifier_CameraShake* EnsureCameraShakeModifier();

private:
	APlayerController* Owner = nullptr;
	TArray<UCameraModifier*> ModifierList;
	UCameraModifier_CameraShake* CameraShakeModifier = nullptr;
	bool bHasValidCameraCachePOV = false;
};
