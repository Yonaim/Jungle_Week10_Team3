#include "PlayerController.h"

#include "GameFramework/PawnActor.h"
#include "Input/InputManager.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Engine/Runtime/Engine.h"
#include "Viewport/GameViewportClient.h"

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

	if (PossessedPawn)
	{
		UCameraComponent* PawnCamera = nullptr;
		for (UActorComponent* Comp : PossessedPawn->GetComponents())
		{
			if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
			{
				PawnCamera = Cam;
				break;
			}
		}

		if (PawnCamera)
		{
			if (UWorld* World = GetWorld())
			{
				World->SetActiveCamera(PawnCamera);
			}

			if (GEngine)
			{
				if (UGameViewportClient* GameVC = GEngine->GetGameViewportClient())
				{
					GameVC->Possess(PawnCamera);
				}
			}
		}
	}
}

void APlayerController::UnPossess()
{
	PossessedPawn = nullptr;
}
