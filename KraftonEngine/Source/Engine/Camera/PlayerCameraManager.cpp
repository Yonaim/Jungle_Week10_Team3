#include "PlayerCameraManager.h"
#include "Engine/GameFramework/PlayerController.h"
#include "Engine/GameFramework/PawnActor.h"

#include <algorithm>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

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
	ApplyCameraModifiers(DeltaTime, ViewTarget.POV);

	// Fade
	if (bEnableFading && FadeTimeRemaining) {
		FadeTimeRemaining = FadeTimeRemaining < 0.f ? 0.f : FadeTimeRemaining;	// Non-negative
		FadeAmount = FadeAlpha.X + (FadeAlpha.Y - FadeAlpha.X) * (FadeTime - FadeTimeRemaining) / FadeTime;
		FadeTimeRemaining -= DeltaTime;
	}


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