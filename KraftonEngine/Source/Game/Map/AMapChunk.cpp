#include "AMapChunk.h"
#include "GameFramework/World.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Game/GameActors/Obstacle/SimpleObstacleActor.h"
#include "Game/GameActors/Obstacle/BarrierObstacleActor.h"
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
	SpawnObstacle();
	BuildFloor();
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
	case EObstacleType::LowBar:	 return World->SpawnActor<ASimpleObstacleActor>();
	case EObstacleType::HighBar: return World->SpawnActor<ASimpleObstacleActor>();
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
		if ((float)rand() / RAND_MAX > ObstacleFillRate) continue;
		if (DecisionSlot.AllowedDecisions.empty()) continue;

		EObstacleDecision Decision = DecisionSlot.AllowedDecisions[rand() % DecisionSlot.AllowedDecisions.size()];

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
		FloorMeshes.push_back(Block);
	}

	// AddComponent does not update the spatial partition, so re-insert the actor
	// so the new floor mesh components become visible to frustum culling.
	if (UWorld* World = GetWorld())
	{
		World->InsertActorToOctree(this);
	}
}
