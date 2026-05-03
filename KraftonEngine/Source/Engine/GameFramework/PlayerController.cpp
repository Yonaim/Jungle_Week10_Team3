#include "PlayerController.h"

#include "GameFramework/PawnActor.h"
#include "Input/InputManager.h"

IMPLEMENT_CLASS(APlayerController, AActor)

APlayerController::APlayerController()
{
	bNeedsTick = true;
	bTickInEditor = false;
}

void APlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetupInputComponent();
}

void APlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	EnhancedInput.ProcessInput(&FInputManager::Get(), DeltaTime, /*bIgnoreGui=*/false);
}

void APlayerController::Possess(APawnActor* InPawn)
{
	if (PossessedPawn == InPawn) return;
	UnPossess();
	PossessedPawn = InPawn;
}

void APlayerController::UnPossess()
{
	PossessedPawn = nullptr;
}
