#pragma once
#include "Game/GameActors/Obstacle/ObstacleActorBase.h"
#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

enum class EChunkType {
	Start,
	Straight,
	//TurnLeft,
	//TurnRight,
	//TwoWay,
	StraightWithHole,
};

struct FObstacleSlot
{
	FVector			LocalPosition;  // where in the chunk this obstacle can appear
	EObstacleType	AllowedTypes;   // bitmask - which obstacle types fit here
};

struct FFloorBlock
{
	FVector LocalPosition = FVector(0.0f, 0.0f, 0.0f);
	FRotator LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);
};

 /*
 At spawn, AMapChunk::BeginPlay() iterates ObstacleSlots, 
 rolls against ObstacleFillRate for each, and calls World->SpawnActor<>() for the winners. 
 This gives runtime variety while the template guarantees at least one lane always stays open
 */
struct FMapChunkTemplate {
	EChunkType				ChunkType = EChunkType::Straight;
	float					Length = 0.0f;
	FVector					ExitOffset = FVector(0.0f, 0.0f, 0.0f);	// local-space offset from entry to next chunk's origin
	FRotator				ExitRotation = FRotator(0.0f, 0.0f, 0.0f);	// e.g. (0, 0, -90) for a left turn around Z
	TArray<FObstacleSlot>	ObstacleSlots;						// available slots, defined per template

	TArray<FFloorBlock> FloorBlockInfos;
};
