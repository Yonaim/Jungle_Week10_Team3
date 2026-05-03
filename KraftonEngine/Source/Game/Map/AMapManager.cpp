#include "AMapManager.h"
#include "GameFramework/World.h"
#include <random>

IMPLEMENT_CLASS(AMapManager, AActor)

AMapManager::AMapManager()
{
	bTickInEditor = true;
	BuildTemplateLibrary();
}

void AMapManager::BeginPlay() {
	AActor::BeginPlay();
	BuildTemplateLibrary();
}

void AMapManager::EndPlay() {
	AActor::EndPlay();
}

void AMapManager::Tick(float DeltaTime) {
	if (!Player) return; 
	if (Templates.empty()) return;

	while ((int32)ActiveChunks.size() < TargetChunkCount) {
		if (ActiveChunks.empty()) {
			SpawnNextChunk(true);
		}
		SpawnNextChunk();
	}

	if (!ActiveChunks.empty())
	{
		AMapChunk* Front = ActiveChunks[0];
		FQuat ExitQuat = FQuat::FromRotator(Front->GetExitRotation());
		FVector ToPlayer = Player->GetActorLocation() - Front->GetExitLocation();
		float ExitProgress = ToPlayer.Dot(ExitQuat.GetForwardVector());

		if (ExitProgress > 0.0f)
			DespawnFrontChunk();
	}
}

void AMapManager::BuildTemplateLibrary() {
	constexpr float ChunkWidth  = 6.f;	// Recommended multiple of 3
	constexpr float ChunkLength = 20.f; // Recommended multiple of 2
	constexpr float TurnLength = 10.f;
	constexpr float LaneY[3] = { -ChunkWidth / 1.5, 0, ChunkWidth / 1.5f };

	Templates.clear();

	//-----------------------------------------------------------------
	// Start
	//-----------------------------------------------------------------
	FMapChunkTemplate Start;
	Start.ChunkType = EChunkType::Start;
	Start.Length = 10.f;
	Start.ExitOffset = FVector(10.f, 0.f, 0.f);

	// Start Floor infos
	FFloorBlock StartFloor = {};
	StartFloor.LocalPosition = FVector(5.f, 0, 0);
	StartFloor.LocalRotation = FRotator(0, 0, 0);
	StartFloor.Scale		 = FVector(5.f, ChunkWidth, 1);
	Start.FloorBlockInfos.push_back(StartFloor);
	Templates.push_back(Start);
	// No obstacles should spawn in the start region; leave the array blank

	//-----------------------------------------------------------------
	// Straight
	//-----------------------------------------------------------------
	FMapChunkTemplate Straight;
	Straight.ChunkType = EChunkType::Straight;
	Straight.Length = ChunkLength;
	Straight.ExitOffset = FVector(ChunkLength, 0.0f, 0.0f);

	// Straight floor infos
	FFloorBlock StraightFloor = {};
	StraightFloor.LocalPosition = FVector(ChunkLength * 0.5f, 0, 0);
	StraightFloor.LocalRotation = FRotator(0, 0, 0);
	StraightFloor.Scale			= FVector(ChunkLength * 0.5, ChunkWidth, 1);
	Straight.FloorBlockInfos.push_back(StraightFloor);

	constexpr uint8 AllTypes = 0xFF;
	float LeftLaneY  = -ChunkWidth / 1.5;
	float MidLaneY   = 0.f;
	float RightLaneY = ChunkWidth / 1.5f;
	for (float X = 2.f; X <= Straight.Length - 2.f; X += 2.f)
		for (uint8 i = 0; i < 3; i++)
		{
			FObstacleSlot Slot{};
			Slot.LocalPosition = FVector(X, LaneY[i], 1.f);
			Slot.AllowedTypes  = static_cast<EObstacleType>(AllTypes);
			Straight.ObstacleSlots.push_back(Slot);
		}

	Templates.push_back(Straight);

	//-----------------------------------------------------------------
	// Left Turn
	//-----------------------------------------------------------------
	//FMapChunkTemplate LeftTurn;
	//LeftTurn.ChunkType = EChunkType::TurnLeft;
	//LeftTurn.Length = ChunkLength;
	//LeftTurn.ExitOffset = FVector(TurnLength, -TurnLength, 0.0f);
	//LeftTurn.ExitRotation = FRotator(0.0f, -90.0f, 0.0f);  // Yaw -90 for left

	//// Left Turn floor
	//FFloorBlock LeftTurnFloorStraight = {};
	//LeftTurnFloorStraight.LocalPosition = FVector(TurnLength * 0.5f, 0, 0);
	//LeftTurnFloorStraight.LocalRotation = FRotator(0, 0, 0);
	//LeftTurnFloorStraight.Scale			= FVector(TurnLength, ChunkWidth, 1);
	//LeftTurn.FloorBlockInfos.push_back(LeftTurnFloorStraight);

	//FFloorBlock LeftTurnFloorCorner = {};
	//LeftTurnFloorCorner.LocalPosition = FVector(TurnLength + ChunkWidth * 0.25f, -ChunkWidth * 0.25f, 0);
	//LeftTurnFloorCorner.LocalRotation = FRotator(0, 0, 0);
	//LeftTurnFloorCorner.Scale		 = FVector(ChunkWidth * 0.5f, ChunkWidth * 0.5f, 1);
	//LeftTurn.FloorBlockInfos.push_back(LeftTurnFloorCorner);

	//FFloorBlock LeftTurnFloorExit = {};
	//LeftTurnFloorExit.LocalPosition = FVector(TurnLength, -(TurnLength + ChunkWidth * 0.5f) * 0.5f, 0);
	//LeftTurnFloorExit.LocalRotation = FRotator(0, -90, 0);
	//LeftTurnFloorExit.Scale			= FVector(TurnLength - ChunkWidth * 0.5f, ChunkWidth, 1);
	//LeftTurn.FloorBlockInfos.push_back(LeftTurnFloorExit);


	//Templates.push_back(LeftTurn);

	//-----------------------------------------------------------------
	// Right Turn
	//-----------------------------------------------------------------
	//FMapChunkTemplate RightTurn;
	//RightTurn.ChunkType = EChunkType::TurnRight;
	//RightTurn.Length = ChunkLength;
	//RightTurn.ExitOffset = FVector(TurnLength, TurnLength, 0.f);
	//RightTurn.ExitRotation = FRotator(0.0f, 90.0f, 0.f);

	//// Right Turn Floor
	//FFloorBlock RightTurnFloorStraight = {};
	//RightTurnFloorStraight.LocalPosition = FVector(TurnLength * 0.5f, 0, 0);
	//RightTurnFloorStraight.LocalRotation = FRotator(0, 0, 0);
	//RightTurnFloorStraight.Scale		 = FVector(TurnLength, ChunkWidth, 1);
	//RightTurn.FloorBlockInfos.push_back(RightTurnFloorStraight);

	//FFloorBlock RightTurnFloorCorner = {};
	//RightTurnFloorCorner.LocalPosition = FVector(TurnLength + ChunkWidth * 0.25f, ChunkWidth * 0.25f, 0);
	//RightTurnFloorCorner.LocalRotation = FRotator(0, 0, 0);
	//RightTurnFloorCorner.Scale		 = FVector(ChunkWidth * 0.5f, ChunkWidth * 0.5f, 1);
	//RightTurn.FloorBlockInfos.push_back(RightTurnFloorCorner);

	//FFloorBlock RightTurnFloorExit = {};
	//RightTurnFloorExit.LocalPosition = FVector(TurnLength, (TurnLength + ChunkWidth * 0.5f) * 0.5f, 0);
	//RightTurnFloorExit.LocalRotation = FRotator(0, 90, 0);
	//RightTurnFloorExit.Scale		 = FVector(TurnLength - ChunkWidth * 0.5f, ChunkWidth, 1);
	//RightTurn.FloorBlockInfos.push_back(RightTurnFloorExit);

	//Templates.push_back(RightTurn);

	//-----------------------------------------------------------------
	// Straight With Hole
	//-----------------------------------------------------------------
	FMapChunkTemplate StraightWithHole;
	StraightWithHole.ChunkType = EChunkType::StraightWithHole;
	StraightWithHole.Length = ChunkLength / 2;
	StraightWithHole.ExitOffset = FVector(StraightWithHole.Length, 0.f, 0.f);

	// Straight With Hole Floor
	FFloorBlock StraightWithHoleFloor1 = {};
	StraightWithHoleFloor1.LocalPosition = FVector(StraightWithHole.Length / 8.f, 0, 0);
	StraightWithHoleFloor1.LocalRotation = FRotator(0, 0, 0);
	StraightWithHoleFloor1.Scale		 = FVector(StraightWithHole.Length / 8, ChunkWidth, 1);
	StraightWithHole.FloorBlockInfos.push_back(StraightWithHoleFloor1);

	FFloorBlock StraightWithHoleFloor2 = {};
	StraightWithHoleFloor2.LocalPosition = FVector(StraightWithHole.Length * 0.75f, 0, 0);
	StraightWithHoleFloor2.LocalRotation = FRotator(0, 0, 0);
	StraightWithHoleFloor2.Scale		 = FVector(StraightWithHole.Length / 4, ChunkWidth, 1);
	StraightWithHole.FloorBlockInfos.push_back(StraightWithHoleFloor2);

	Templates.push_back(StraightWithHole);
}

void AMapManager::SpawnNextChunk(bool Init)
{
	int32 Idx = SelectNextTemplateIndex();
	const FMapChunkTemplate& T = Init ? Templates[0] : Templates[Idx];

	FVector  SpawnLoc = ActiveChunks.empty() ? FVector(0, 0, 0) : ActiveChunks.back()->GetExitLocation();
	FRotator SpawnRot = ActiveChunks.empty() ? FRotator(0, 0, 0) : ActiveChunks.back()->GetExitRotation();

	AMapChunk* Chunk = GetWorld()->SpawnActor<AMapChunk>();
	Chunk->SetActorLocation(SpawnLoc);
	Chunk->SetActorRotation(SpawnRot);

	// TODO: Obstacle fill rate should ramp with time logarithmically, clamped to some reasonable value
	Chunk->InitFromTemplate(T, 0.2);
	ActiveChunks.push_back(Chunk);

	//bool bIsTurn = (T.ChunkType == EChunkType::TurnLeft || T.ChunkType == EChunkType::TurnRight);
	//StraightRunLength = bIsTurn ? 0 : StraightRunLength + 1;
}

void AMapManager::DespawnFrontChunk() {
	if (ActiveChunks.empty()) return;
	GetWorld()->DestroyActor(ActiveChunks.front());
	ActiveChunks.erase(ActiveChunks.begin());
}

int32 AMapManager::SelectNextTemplateIndex()
{
	TArray<int32> Candidates;
	for (int32 i = 1; i < (int32)Templates.size(); ++i)
	{
		EChunkType T = Templates[i].ChunkType;
		//bool bIsTurn = (T == EChunkType::TurnLeft || T == EChunkType::TurnRight);
		//if (bIsTurn && StraightRunLength < MinStraightsBetweenTurns)
		//	continue;
		Candidates.push_back(i);
	}

	return Candidates[rand() % Candidates.size()];
}
