#pragma once
#include "GameFramework/AActor.h"
#include "AMapChunk.h"

class AMapManager : public AActor
{
public:
	DECLARE_CLASS(AMapManager, AActor)

	AMapManager();

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void EndPlay() override;

	void SetPlayerActor(AActor* InPlayer) { Player = InPlayer; }

private:
	void  BuildTemplateLibrary();
	void  SpawnNextChunk(bool Init = false);
	void  DespawnFrontChunk();
	int32 SelectNextTemplateIndex();

	TArray<FMapChunkTemplate> Templates;
	TArray<AMapChunk*>        ActiveChunks;
	AActor* Player = nullptr;

	int32 StraightRunLength = 0;
	int32 MinStraightsBetweenTurns = 2;
	int32 TargetChunkCount = 6;
};