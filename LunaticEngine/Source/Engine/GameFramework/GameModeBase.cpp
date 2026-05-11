#include "GameModeBase.h"

#include "Engine/Camera/PlayerCameraManager.h"
#include "GameFramework/PawnActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Core/Log.h"
IMPLEMENT_CLASS(AGameModeBase, AActor)

namespace
{
	APawnActor* FindFirstPlacedPawn(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (AActor* Actor : World->GetActors())
		{
			APawnActor* Pawn = Cast<APawnActor>(Actor);
			if (Pawn && IsAliveObject(Pawn))
			{
				return Pawn;
			}
		}

		return nullptr;
	}
}

void AGameModeBase::StartPlay()
{
	UWorld* World = GetTypedOuter<UWorld>();
	if (!World)
	{
		// Outer 체인이 World까지 안 닿으면 GEngine을 통해 fallback
		World = GEngine ? GEngine->GetWorld() : nullptr;
	}
	if (!World) return;

	//  PlayerController 스폰
	if (!PlayerControllerClassName.empty())
	{
		UObject* Obj = FObjectFactory::Get().Create(PlayerControllerClassName, World->GetCurrentLevel());
		SpawnedController = Cast<APlayerController>(Obj);
		if (SpawnedController)
		{
			World->AddActor(SpawnedController);
		}
		else if (Obj)
		{
			UObjectManager::Get().DestroyObject(Obj);
		}
	}

	//  배치된 Pawn이 있으면 우선 Possess하고, 없을 때만 DefaultPawn을 스폰한다.
	SpawnedPawn = FindFirstPlacedPawn(World);
	if (!SpawnedPawn && !DefaultPawnClassName.empty())
	{
		UObject* Obj = FObjectFactory::Get().Create(DefaultPawnClassName, World->GetCurrentLevel());
		SpawnedPawn = Cast<APawnActor>(Obj);
		if (SpawnedPawn)
		{
			World->AddActor(SpawnedPawn);
		}
		else if (Obj)
		{
			UObjectManager::Get().DestroyObject(Obj);
		}
	}

	if (SpawnedController && SpawnedPawn)
	{
		SpawnedController->Possess(SpawnedPawn);
	}

	if (!PlayerCameraManagerClassName.empty()) {
		UObject* Obj = FObjectFactory::Get().Create(PlayerCameraManagerClassName, World->GetCurrentLevel());
		CameraManager = Cast<APlayerCameraManager>(Obj);
		if (CameraManager) 
		{
			World->AddActor(CameraManager);
			if (SpawnedController) {
				SpawnedController->AcquirePlayerCameraManager(CameraManager);
			}

		}
		else if (Obj)
		{
			UObjectManager::Get().DestroyObject(Obj);
		}
	}
}
