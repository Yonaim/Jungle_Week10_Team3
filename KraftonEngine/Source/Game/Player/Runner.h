#pragma once
#include "Engine/GameFramework/PawnActor.h"

class UScriptComponent;
class UCameraComponent;
class UBoxComponent;
class UStaticMeshComponent;
/*
* Jungle Run (가제)에서 조작 대상 Player
*/
class ARunner : public APawnActor
{
public:
	DECLARE_CLASS(ARunner, APawnActor)
	ARunner();
	// Play
	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay() override;

	void Serialize(FArchive& Ar) override;
	UObject* Duplicate(UObject* NewOuter = nullptr) const override;

protected:
	void TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

private:
	void ApplyDefaultVisual();
	void SetupCamera();

	// RootComponent (Box)
	UBoxComponent* BoxComponent = nullptr;
	// Script
	UScriptComponent* PlayerMove = nullptr;
	// Camera
	UCameraComponent* MainCamera = nullptr;
	// StaticMesh
	UStaticMeshComponent* MeshComponent = nullptr;
};

