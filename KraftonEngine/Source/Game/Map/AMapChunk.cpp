#include "AMapChunk.h"
#include "GameFramework/World.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Game/GameActors/Obstacle/SimpleObstacleActor.h"
#include "Game/GameActors/Obstacle/BarrierObstacleActor.h"
#include "Game/GameActors/Obstacle/MustJumpObstacleActor.h"
#include "Game/GameActors/Obstacle/MustSlideObstacleActor.h"
#include "Game/GameActors/Obstacle/WireballActor.h"
#include "Game/Map/MapRandom.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"

IMPLEMENT_CLASS(AMapChunk, AActor)

void AMapChunk::BeginPlay() {
	AActor::BeginPlay();
}

void AMapChunk::EndPlay() {
	for (auto* Obstacle : SpawnedObstacles) {
		if (Obstacle && Obstacle->GetWorld() && Obstacle->GetWorld()->HasBegunPlay()) {
			Obstacle->GetWorld()->DestroyActor(Obstacle);
		}
	}

	SpawnedObstacles.clear();
	AActor::EndPlay();
}

void AMapChunk::InitFromTemplate(const FMapChunkTemplate& InTemplate, float InObstacleFillRate) {
	Template         = InTemplate;
	ObstacleFillRate = InObstacleFillRate;
	BuildFloor();
	SpawnObstacle();
}

FVector AMapChunk::GetExitLocation() const
{
	FQuat WorldQuat = FQuat::FromRotator(GetActorRotation());
	return GetActorLocation() + WorldQuat.RotateVector(Template.ExitOffset);
}

FRotator AMapChunk::GetExitRotation() const
{
	FQuat WorldQuat = FQuat::FromRotator(GetActorRotation());
	FQuat ExitQuat = FQuat::FromRotator(Template.ExitRotation);
	return (WorldQuat * ExitQuat).ToRotator();
}


static AObstacleActorBase* SpawnObstacleOfType(UWorld* World, EObstacleType Type)
{
	if (!World)
	{
		return nullptr;
	}

	// Using SimpleObstacleActor as placeholder
	switch (Type)
	{
	case EObstacleType::Barrier: return World->SpawnActor<ABarrierObstacleActor>();
	case EObstacleType::LowBar:	 return World->SpawnActor<AMustJumpObstacleActor>();
	case EObstacleType::HighBar: return World->SpawnActor<AMustSlideObstacleActor>();
	case EObstacleType::Misc:    return World->SpawnActor<ASimpleObstacleActor>();
	default:                     return nullptr;
	}
}

static AObstacleActorBase* SpawnObstacleAt(UWorld* World, EObstacleType Type, const FVector& Location)
{
	AObstacleActorBase* Obstacle = SpawnObstacleOfType(World, Type);
	if (!Obstacle)
	{
		return nullptr;
	}

	Obstacle->InitDefaultComponents("");
	Obstacle->SetTag("Obstacle");
	Obstacle->SetActorLocation(Location);
	World->InsertActorToOctree(Obstacle);

	if (!Obstacle->HasActorBegunPlay())
	{
		Obstacle->BeginPlay();
	}

	return Obstacle;
}

static FString GetMeshPath(const char* MeshKey)
{
	if (const FMeshResource* MeshResource = FResourceManager::Get().FindMesh(FName(MeshKey)))
	{
		return MeshResource->Path;
	}

	return "";
}

static void ApplyBasicShapeMaterial(UStaticMeshComponent* MeshComponent, UStaticMesh* Mesh)
{
	if (!MeshComponent || !Mesh)
	{
		return;
	}

	const FString MaterialPath = FResourceManager::Get().ResolvePath(FName("Default.Material.BasicShape"));
	UMaterial* Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
	if (!Material)
	{
		return;
	}

	int32 MaterialCount = static_cast<int32>(Mesh->GetStaticMaterials().size());
	if (MaterialCount == 0 && Mesh->GetStaticMeshAsset() &&
		(!Mesh->GetStaticMeshAsset()->Sections.empty() || !Mesh->GetStaticMeshAsset()->Indices.empty()))
	{
		MaterialCount = 1;
	}

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		MeshComponent->SetMaterial(MaterialIndex, Material);
	}
}

static void ApplyCubeMesh(UStaticMeshComponent* MeshComponent)
{
	if (!MeshComponent || !GEngine)
	{
		return;
	}

	const FString MeshPath = GetMeshPath("Default.Mesh.BasicShape.Cube");
	if (MeshPath.empty())
	{
		return;
	}

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(MeshPath, Device);
	MeshComponent->SetStaticMesh(Mesh);
	ApplyBasicShapeMaterial(MeshComponent, Mesh);
}

void AMapChunk::SpawnObstacle()
{
	FQuat WorldQuat = FQuat::FromRotator(GetActorRotation());
	const float LaneY[3] = { -Template.Width / 1.5f, 0.0f, Template.Width / 1.5f };
	constexpr float ObstacleZ = 1.0f;

	for (const FDecisionSlot& DecisionSlot : Template.ObstacleSlotDecisions) {
		if (!MapRandom::Chance(ObstacleFillRate)) continue;
		if (DecisionSlot.AllowedDecisions.empty()) continue;

		const int32 DecisionIndex = MapRandom::Index(static_cast<int32>(DecisionSlot.AllowedDecisions.size()));
		EObstacleDecision Decision = DecisionSlot.AllowedDecisions[DecisionIndex];

		auto WorldPositionForLane = [&](int32 LaneIndex)
		{
			return GetActorLocation() + WorldQuat.RotateVector(FVector(DecisionSlot.X, LaneY[LaneIndex], ObstacleZ));
		};

		switch (Decision) {
		case (SingleBarrierLeft):
		{
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(0)))
				SpawnedObstacles.push_back(Obs);
			break;
		}
		case (SingleBarrierMiddle):
		{
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(1)))
				SpawnedObstacles.push_back(Obs);
			break;
		}
		case (SingleBarrierRight):
		{
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(2)))
				SpawnedObstacles.push_back(Obs);
			break;
		}
		case (DoubleBarrierLeft):
		{
			if (AObstacleActorBase* Obs0 = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(0)))
				SpawnedObstacles.push_back(Obs0);
			if (AObstacleActorBase* Obs1 = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(1)))
				SpawnedObstacles.push_back(Obs1);
			break;
		}
		case (DoubleBarrierRight):
		{
			if (AObstacleActorBase* Obs0 = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(2)))
				SpawnedObstacles.push_back(Obs0);
			if (AObstacleActorBase* Obs1 = SpawnObstacleAt(GetWorld(), EObstacleType::Barrier, WorldPositionForLane(1)))
				SpawnedObstacles.push_back(Obs1);
			break;
		}
		case (MustJump):
		{
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::LowBar, WorldPositionForLane(1)))
				SpawnedObstacles.push_back(Obs);
			break;
		}
		case (MustSlide):
		{
			if (AObstacleActorBase* Obs = SpawnObstacleAt(GetWorld(), EObstacleType::HighBar, WorldPositionForLane(1)))
				SpawnedObstacles.push_back(Obs);
			break;
		}
		default: {
			break;
		}
		}
	}
}

void AMapChunk::BuildFloor() {
	for (UStaticMeshComponent* FloorMesh : FloorMeshes)
	{
		RemoveComponent(FloorMesh);
	}
	FloorMeshes.clear();

	for (const FFloorBlock& BlockInfo : Template.FloorBlockInfos) {
		UStaticMeshComponent* Block = AddComponent<UStaticMeshComponent>();
		if (GetRootComponent())
		{
			Block->AttachToComponent(GetRootComponent());
		}

		ApplyCubeMesh(Block);
		FVector BlockPos = BlockInfo.LocalPosition;
		Block->SetRelativeLocation(FVector(BlockPos.X, BlockPos.Y, BlockPos.Z));
		Block->SetRelativeRotation(BlockInfo.LocalRotation);
		Block->SetRelativeScale(BlockInfo.Scale);

		// PlayerController의 FindGround()는 collision enabled primitive만 바닥 후보로 본다.
		// Floor는 장애물처럼 데미지를 주는 대상은 아니지만, Player가 "딛고 설 수 있는 지형"이다.
		// 따라서 collision을 명시적으로 켜고, overlap도 켜서 디버그 중 상태를 쉽게 확인할 수 있게 한다.
		// Mesh/Transform/Scale을 모두 적용한 뒤 bounds를 dirty 처리해야 다음 ground query에서
		// 최신 world AABB, 특히 바닥 상단 Z값이 정확히 계산된다.
		Block->SetCollisionEnabled(true);
		Block->SetGenerateOverlapEvents(true);
		Block->MarkWorldBoundsDirty();

		FloorMeshes.push_back(Block);
	}

	// AddComponent는 Actor의 spatial partition/octree 상태를 자동으로 완전히 갱신하지 않는다.
	// FloorMesh를 런타임에 붙인 뒤 Actor를 다시 octree에 넣어야 렌더링/피킹/충돌 broad-phase에서
	// 새 floor component들이 안정적으로 관찰된다.
	if (UWorld* World = GetWorld())
	{
		World->InsertActorToOctree(this);
	}
}
