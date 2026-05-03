#pragma once
#include "GameFramework/StaticMeshActor.h"  
#include "Core/CollisionEventTypes.h"  
#include <functional>  

class UShapeComponent;
class UBoxComponent;
class USphereComponent;
class UCapsuleComponent;

class AObstacleActorBase : public AStaticMeshActor {
public:
	DECLARE_CLASS(AObstacleActorBase, AStaticMeshActor)
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime) {};
	virtual void EndPlay() {};

	virtual void InitDefaultComponents(const FString& UStaticMeshFileName) override;

protected:
	virtual ~AObstacleActorBase() = default;

	//---------------------------------------------------------------------------  
	// The "Hitter" identifies the "Victim", but the Victim decides how to bleed.  
	//---------------------------------------------------------------------------  

	// Notify the Player that it is hit by an obstacle.   
	virtual void OnHit(const FComponentHitEvent& Other);

	// Notify the Player that it is overlapping with an obstacle  
	virtual void OnOverlap(const FComponentOverlapEvent& Other);

	// Should be called when a player collides into one of its shape components.  
	// TODO: Add a concrete definition once player class is ready  
	virtual void OnPlayerCollision() = 0;

protected:
	float OnHitDamage = 0.f;
};