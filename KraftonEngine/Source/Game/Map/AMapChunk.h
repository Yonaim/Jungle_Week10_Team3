#pragma once
#include "GameFramework/AActor.h"
#include "Game/Map/MapChunkTemplate.h"

class USceneComponent;

class AMapChunk : public AActor {
public:
	DECLARE_CLASS(AMapChunk, AActor)

	void BeginPlay() override;
	void EndPlay()	 override;
	void InitFromTemplate(const FMapChunkTemplate& InTemplate, float ObstacleFillRate);

	FVector    GetExitLocation() const;
	FRotator   GetExitRotation() const;
	float      GetChunkLength()  const { return Template.Length; }
	EChunkType GetChunkType()    const { return Template.ChunkType; }

private:
	// Random Obstacle generator
	void SpawnObstacle();

	// Builds floor based on the FloorBlockInfos array in the template 
	void BuildFloor();

private:
	FMapChunkTemplate     Template;
	float                 ObstacleFillRate = 0.f;
	USceneComponent*      ChunkRoot = nullptr;
	TArray<UStaticMeshComponent*> FloorMeshes;
	TArray<AObstacleActorBase*> SpawnedObstacles;
};
