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
	return POV->GetUUID() == OtherTarget.POV->GetUUID();
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{

}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* InModifier) {
	if (!InModifier) return;
	ModifierList.push_back(InModifier);
	std::sort(ModifierList.begin(), ModifierList.end(), [](const UCameraModifier* A, const UCameraModifier* B) {
		return A->Priority >= B->Priority;	
	});
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, UCameraComponent* InOutPOV) {
	for (auto* CameraModifier : ModifierList){
		if (CameraModifier)
			CameraModifier->ModifyCamera(DeltaTime, *InOutPOV);
	}
}