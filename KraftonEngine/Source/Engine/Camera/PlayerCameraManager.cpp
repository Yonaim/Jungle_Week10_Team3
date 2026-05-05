#include "PlayerCameraManager.h"
#include "Engine/GameFramework/PlayerController.h"

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
