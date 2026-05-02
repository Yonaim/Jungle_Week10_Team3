#pragma once
#include "AActor.h"
#include "Core/CoreTypes.h"

class APawnActor;
class APlayerController;

class AGameModeBase : public AActor
{
public:
	DECLARE_CLASS(AGameModeBase, AActor);

	// from StartPlay DefaultPawn / PlayerController spawn, Possess actor
	virtual void StartPlay();

	// subclass consturctor default value( DefaultPawnClass / PlayerControllerClass)
	FString DefaultPawnClassName = "APawnActor";
	FString PlayerControllerClassName = "APlayerController";

	APawnActor* GetSpawnedPawn() const { return SpawnedPawn; }
	APlayerController* GetSpawnedController() const { return SpawnedController; }

protected:
	APawnActor* SpawnedPawn = nullptr;
	APlayerController* SpawnedController = nullptr;
};
