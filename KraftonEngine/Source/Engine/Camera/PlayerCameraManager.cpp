#include "PlayerCameraManager.h"
#include "Engine/GameFramework/PlayerController.h"
#include "Engine/GameFramework/PawnActor.h"
#include "Object/Object.h"
#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"

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
	return Target != nullptr && Target == OtherTarget.Target;
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{
	if (!Target && OwningController)
	{
		Target = OwningController->GetPawn();
	}
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{

}

void APlayerCameraManager::UpdateCamera(float deltaTime)
{
	LastFrameCameraCache = CameraCache;

	ViewTarget.CheckViewTarget(Owner);
	FMinimalViewInfo NewPOV;
	if (UCameraComponent* Camera = FindCameraComponent(ViewTarget.Target))
	{
		Camera->GetCameraView(deltaTime, NewPOV);
	}
	else
	{
		NewPOV = BuildFallbackCameraView(ViewTarget.Target);
	}


	ApplyCameraModifiers(deltaTime, NewPOV);

	ViewTarget.POV = NewPOV;
	CameraCache.TimeStamp += deltaTime;
	CameraCache.POV = NewPOV;
}
//
UCameraComponent* APlayerCameraManager::FindCameraComponent(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}
	return Target->GetComponentByClass<UCameraComponent>();

}
// Build to CameraView if Target is nullptr make default FMinimalViewInfo
FMinimalViewInfo APlayerCameraManager::BuildFallbackCameraView(AActor* Target) const
{
	FMinimalViewInfo ViewInfo;
	if (Target)
	{
		ViewInfo.Location = Target->GetActorLocation();
		ViewInfo.Rotation = Target->GetActorRotation();
	}
	else
	{
		ViewInfo.Location = FVector::ZeroVector;
		ViewInfo.Rotation = FRotator::ZeroRotator;
	}
	ViewInfo.FOV = 3.14159265358979f / 3.0f;
	ViewInfo.NearZ = 0.1f;
	ViewInfo.FarZ = 1000.0f;
	ViewInfo.bIsOrthogonal = false;
	ViewInfo.OrthoWidth = 10.0f;
	ViewInfo.PostProcessBlendWeight = 1.0f;

	return ViewInfo;
}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* InModifier) {
	if (!InModifier) return;
	ModifierList.push_back(InModifier);
	std::sort(ModifierList.begin(), ModifierList.end(), [](const UCameraModifier* A, const UCameraModifier* B) {
		return A->Priority >= B->Priority;	
	});
}

