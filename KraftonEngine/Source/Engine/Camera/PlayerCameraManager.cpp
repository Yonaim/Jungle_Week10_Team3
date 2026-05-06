#include "PlayerCameraManager.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetCurveUtils.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Camera/CameraShakePattern.h"
#include "Engine/Core/Log.h"
#include "Engine/GameFramework/PlayerController.h"
#include "Engine/GameFramework/PawnActor.h"
#include "Object/ObjectFactory.h"

#include <algorithm>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

namespace
{
	UCameraModifier* BuildModifierFromAssetDesc(const FCameraShakeModifierAssetDesc& Desc)
	{
		UCameraModifier* Modifier = UObjectManager::Get().CreateObject<UCameraModifier>();
		if (!Modifier)
		{
			return nullptr;
		}

		UCameraShakePattern* Pattern = UObjectManager::Get().CreateObject<UCameraShakePattern>();
		if (!Pattern)
		{
			UObjectManager::Get().DestroyObject(Modifier);
			return nullptr;
		}

		Pattern->SetTransitionCurveX(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationX, Desc.LocationAmplitude.X * Desc.Intensity));
		Pattern->SetTransitionCurveY(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationY, Desc.LocationAmplitude.Y * Desc.Intensity));
		Pattern->SetTransitionCurveZ(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationZ, Desc.LocationAmplitude.Z * Desc.Intensity));
		Pattern->SetRotationCurveX(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationX, Desc.RotationAmplitude.Roll * Desc.Intensity));
		Pattern->SetRotationCurveY(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationY, Desc.RotationAmplitude.Pitch * Desc.Intensity));
		Pattern->SetRotationCurveZ(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationZ, Desc.RotationAmplitude.Yaw * Desc.Intensity));

		Modifier->Priority = Desc.Common.Priority;
		Modifier->SetAlphaInTime(Desc.Common.AlphaInTime);
		Modifier->SetAlphaOutTime(Desc.Common.AlphaOutTime);
		Modifier->SetCameraShakePattern(Pattern);
		return Modifier;
	}
}

void FViewTarget::SetNewTarget(AActor* NewTarget)
{
	if (!NewTarget) return;
	Target = NewTarget;
}

APawnActor* FViewTarget::GetTargetPawn() const
{
	if (!Target) return nullptr;
	return Cast<APawnActor>(Target);
}

bool FViewTarget::Equal(const FViewTarget& OtherTarget) const
{
	if (Target && OtherTarget.Target) {
		return Target == OtherTarget.Target;
	}

	return false;
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{

}

void APlayerCameraManager::BeginPlay()
{
	AActor::BeginPlay();
	LoadCameraModifierStackAsset("Asset/CameraShakeStack.uasset");
}

void APlayerCameraManager::EndPlay()
{
	for (UCameraModifier* Modifier : ModifierList) {
		if (!Modifier) continue;
		UObjectManager::Get().DestroyObject(Modifier);
	}
	ModifierList.clear();
	AActor::EndPlay();
}

void APlayerCameraManager::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	ApplyCameraModifiers(DeltaTime, ViewTarget.POV);
}

void APlayerCameraManager::UpdateCamera(float DeltaTime) {
	// Reset Camera Snapshot
	if (ViewTarget.GetTargetPawn()) {
		APawnActor* Pawn = ViewTarget.GetTargetPawn();
		for (UActorComponent* Comp : Pawn->GetComponents())
		{
			if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
			{
				ViewTarget.POV = Cam->GetCameraState();
			}
		}
	}

	ApplyCameraModifiers(DeltaTime, ViewTarget.POV);

	// Fade
	if (bEnableFading && FadeTimeRemaining) {
		FadeTimeRemaining = FadeTimeRemaining < 0.f ? 0.f : FadeTimeRemaining;	// Non-negative
		FadeAmount = FadeAlpha.X + (FadeAlpha.Y - FadeAlpha.X) * (FadeTime - FadeTimeRemaining) / FadeTime;
		FadeTimeRemaining -= DeltaTime;
	}

	ViewTarget.POV.PostProcessSettings.FadeColor = FadeColor;
	ViewTarget.POV.PostProcessSettings.FadeAmount = FadeAmount;
}

void APlayerCameraManager::LoadCameraModifierStackAsset(const std::filesystem::path& AssetPath)
{
	FString Error;
	UAssetData* LoadedAsset = FAssetFileSerializer::LoadAssetFromFile(AssetPath, &Error);
	if (!LoadedAsset)
	{
		UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to load camera modifier asset: %s", Error.c_str());
		return;
	}

	UCameraModifierStackAssetData* StackAsset = Cast<UCameraModifierStackAssetData>(LoadedAsset);
	if (!StackAsset)
	{
		UE_LOG_CATEGORY(PlayerCameraManager, Error, "Asset type mismatch: %s", AssetPath.string().c_str());
		UObjectManager::Get().DestroyObject(LoadedAsset);
		return;
	}

	for (const FCameraShakeModifierAssetDesc& Desc : StackAsset->CameraShakes)
	{
		UCameraModifier* Modifier = BuildModifierFromAssetDesc(Desc);
		if (!Modifier)
		{
			UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to create camera modifier from asset entry: %s", Desc.Name.c_str());
			continue;
		}

		AddCameraModifier(Modifier);
		if (Desc.Common.bStartDisabled)
		{
			Modifier->DisableModifier(true);
		}
	}

	UObjectManager::Get().DestroyObject(LoadedAsset);
}

void APlayerCameraManager::SetOwner(APlayerController* InController) {
	if (!InController) return;
	Owner = InController;
	if (InController->GetPawn()) {
		APawnActor* Pawn = InController->GetPawn();
		ViewTarget.SetNewTarget(Pawn);
	}
}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* InModifier)
{
	if (!InModifier) return;

	for (UCameraModifier* ExistingModifier : ModifierList)
	{
		if (ExistingModifier == InModifier)
		{
			InModifier->EnableModifier();
			return;
		}
	}

	InModifier->AddedToCamera(this);
	InModifier->EnableModifier();
	ModifierList.push_back(InModifier);
	std::sort(ModifierList.begin(), ModifierList.end(), [](const UCameraModifier* A, const UCameraModifier* B) {
		return A->Priority > B->Priority;
	});
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	for (UCameraModifier* CameraModifier : ModifierList)
	{
		if (!CameraModifier || CameraModifier->IsDisabled())
		{
			continue;
		}

		CameraModifier->UpdateAlpha(DeltaTime);
		if (CameraModifier->IsDisabled())
		{
			continue;
		}

		const bool bStopProcessing = CameraModifier->ModifyCamera(DeltaTime, InOutPOV);
		if (bStopProcessing)
		{
			break;
		}
	}
}

void APlayerCameraManager::StartCameraShake() {
	for (UCameraModifier* Modifier : ModifierList) {
		if (!Modifier) continue;
		Modifier->EnableModifier();
	}
}

void APlayerCameraManager::EndCameraShake() {
	for (UCameraModifier* Modifier : ModifierList) {
		if (!Modifier) continue;
		Modifier->DisableModifier();
	}
}

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color) {
	// 호출 시 Fade 정보들을 즉시 매개변수값으로 초기화
	FadeColor		  = Color;
	FadeAlpha		  = FVector2(FromAlpha, ToAlpha);
	FadeTimeRemaining = Duration;
	FadeTime		  = Duration;
	bEnableFading	  = true;
}

void APlayerCameraManager::EndCameraFade() {
	// 호출 시 Fade 즉시 제거 (보간하지 않음)
	bEnableFading	  = false;
	FadeAmount		  = 0.0f;
	FadeTimeRemaining = 0.0f;
}