#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Collision/CollisionDispatcher.h"
#include "Component/Shape/ShapeComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Component/CameraComponent.h"
#include "Render/Types/LODContext.h"
#include <algorithm>
#include <filesystem>
#include "Profiling/Stats.h"
#include "GameFramework/GameModeBase.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"
IMPLEMENT_CLASS(UWorld, UObject)


UWorld::~UWorld()
{
	EndPlay();
}

UObject* UWorld::Duplicate(UObject* NewOuter) const
{
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!NewWorld)
	{
		return nullptr;
	}
	NewWorld->SetOuter(NewOuter);
	NewWorld->InitWorld();

	for (AActor* Src : GetActors())
	{
		if (!Src) continue;
		Src->Duplicate(NewWorld);
	}

	NewWorld->PostDuplicate();
	return NewWorld;
}

UWorld* UWorld::DuplicateAs(EWorldType InWorldType) const
{
	UWorld* NewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!NewWorld) return nullptr;

	NewWorld->SetWorldType(InWorldType);
	NewWorld->InitWorld();

	for (AActor* Src : GetActors())
	{
		if (!Src) continue;
		Src->Duplicate(NewWorld);
	}

	NewWorld->PostDuplicate();
	return NewWorld;
}

void UWorld::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	if (Ar.IsSaving())
	{
		//  Serialize Persistent Level Data (Only persistent level data is stored in this file)
		if (PersistentLevel)
		{
			PersistentLevel->Serialize(Ar);
		}

		// Serialize Streaming Levels Metadata (References to other files)
		int32 StreamingCount = static_cast<int32>(StreamingLevels.size());
		Ar << StreamingCount;
		for (auto& Info : StreamingLevels)
		{
			Ar << Info.LevelPath;
			Ar << Info.LevelName;
			Ar << Info.bShouldBeVisible;
		}
	}
	else if (Ar.IsLoading())
	{
		ClearLevels();
		StreamingLevels.clear();

		// Deserialize Persistent Level Data
		PersistentLevel = UObjectManager::Get().CreateObject<ULevel>(this);
		PersistentLevel->SetWorld(this);
		PersistentLevel->Serialize(Ar);
		Levels.push_back(PersistentLevel);
		CurrentLevel = PersistentLevel;

		// Deserialize Streaming Levels Metadata
		int32 StreamingCount = 0;
		Ar << StreamingCount;
		for (int32 i = 0; i < StreamingCount; ++i)
		{
			FStreamingLevelInfo Info;
			Ar << Info.LevelPath;
			Ar << Info.LevelName;
			Ar << Info.bShouldBeVisible;
			StreamingLevels.push_back(Info);
		}

		// Auto-load streaming levels 
		for (auto& Info : StreamingLevels)
		{
			LoadStreamingLevel(Info.LevelPath);
		}
	}
}

void UWorld::AddStreamingLevel(const FString& LevelPath)
{
	for (const auto& Info : StreamingLevels)
	{
		if (Info.LevelPath == LevelPath) return;
	}

	FStreamingLevelInfo NewInfo;
	NewInfo.LevelPath = LevelPath;
	NewInfo.LevelName = FName(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(LevelPath)).stem().wstring()));
	StreamingLevels.push_back(NewInfo);
	
	LoadStreamingLevel(LevelPath);
}

void UWorld::LoadStreamingLevel(const FString& LevelPath)
{
	FStreamingLevelInfo* TargetInfo = nullptr;
	for (auto& Info : StreamingLevels)
	{
		if (Info.LevelPath == LevelPath)
		{
			TargetInfo = &Info;
			break;
		}
	}

	if (!TargetInfo || TargetInfo->bIsLoaded) return;

	// Load Level from separate file
	ULevel* NewLevel = UObjectManager::Get().CreateObject<ULevel>(this);
	NewLevel->SetWorld(this);

	FWindowsBinReader Ar(LevelPath);
	if (Ar.IsValid())
	{
		NewLevel->Serialize(Ar);
		AddLevel(NewLevel);
		TargetInfo->LoadedLevel = NewLevel;
		TargetInfo->bIsLoaded = true;

		if (bHasBegunPlay) NewLevel->BeginPlay();
	}
	else
	{
		UObjectManager::Get().DestroyObject(NewLevel);
	}
}

void UWorld::UnloadStreamingLevel(const FName& LevelName)
{
	for (auto& Info : StreamingLevels)
	{
		if (Info.LevelName == LevelName && Info.bIsLoaded)
		{
			if (Info.LoadedLevel)
			{
				Info.LoadedLevel->EndPlay();
				RemoveLevel(Info.LoadedLevel);
				UObjectManager::Get().DestroyObject(Info.LoadedLevel);
				Info.LoadedLevel = nullptr;
			}
			Info.bIsLoaded = false;
			return;
		}
	}
}

void UWorld::AddLevel(ULevel* InLevel)
{
	if (!InLevel) return;
	if (std::find(Levels.begin(), Levels.end(), InLevel) == Levels.end())
	{
		InLevel->SetWorld(this);
		Levels.push_back(InLevel);
		if (!PersistentLevel) PersistentLevel = InLevel;
	}
}

void UWorld::RemoveLevel(ULevel* InLevel)
{
	if (!InLevel) return;
	auto it = std::find(Levels.begin(), Levels.end(), InLevel);
	if (it != Levels.end())
	{
		if (CurrentLevel == InLevel) CurrentLevel = (Levels.size() > 1) ? (Levels[0] == InLevel ? Levels[1] : Levels[0]) : nullptr;
		if (PersistentLevel == InLevel) PersistentLevel = (Levels.size() > 1) ? (Levels[0] == InLevel ? Levels[1] : Levels[0]) : nullptr;
		Levels.erase(it);
	}
}

void UWorld::ClearLevels()
{
	for (ULevel* Level : Levels)
	{
		if (Level) UObjectManager::Get().DestroyObject(Level);
	}
	Levels.clear();
	PersistentLevel = nullptr;
	CurrentLevel = nullptr;
}

void UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor) return;
	Actor->EndPlay();
	
	ULevel* OwningLevel = Cast<ULevel>(Actor->GetOuter());
	if (OwningLevel)
	{
		OwningLevel->RemoveActor(Actor);
	}

	MarkWorldPrimitivePickingBVHDirty();
	Partition.RemoveActor(Actor);

	UObjectManager::Get().DestroyObject(Actor);
}

void UWorld::AddActor(AActor* Actor)
{
	if (!Actor || !CurrentLevel)
	{
		return;
	}

	CurrentLevel->AddActor(Actor);

	InsertActorToOctree(Actor);
	MarkWorldPrimitivePickingBVHDirty();

	if (bHasBegunPlay && !Actor->HasActorBegunPlay())
	{
		Actor->BeginPlay();
	}
}

void UWorld::MarkWorldPrimitivePickingBVHDirty()
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.MarkDirty();
}

void UWorld::BuildWorldPrimitivePickingBVHNow() const
{
	WorldPrimitivePickingBVH.BuildNow(GetActors().ToArray());
}

void UWorld::BeginDeferredPickingBVHUpdate()
{
	++DeferredPickingBVHUpdateDepth;
}

void UWorld::EndDeferredPickingBVHUpdate()
{
	if (DeferredPickingBVHUpdateDepth <= 0)
	{
		return;
	}

	--DeferredPickingBVHUpdateDepth;
	if (DeferredPickingBVHUpdateDepth == 0 && bDeferredPickingBVHDirty)
	{
		bDeferredPickingBVHDirty = false;
		BuildWorldPrimitivePickingBVHNow();
	}
}

void UWorld::WarmupPickingData() const
{
	for (AActor* Actor : GetActors())
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive || !Primitive->IsVisible() || !Primitive->IsA<UStaticMeshComponent>())
			{
				continue;
			}

			UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Primitive);
			if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
			{
				StaticMesh->EnsureMeshTrianglePickingBVHBuilt();
			}
		}
	}

	BuildWorldPrimitivePickingBVHNow();
}

bool UWorld::RaycastPrimitives(const FRay& Ray, FRayHitResult& OutHitResult, AActor*& OutActor) const
{
	WorldPrimitivePickingBVH.EnsureBuilt(GetActors().ToArray());
	return WorldPrimitivePickingBVH.Raycast(Ray, OutHitResult, OutActor);
}

void UWorld::InsertActorToOctree(AActor* Actor)
{
	Partition.InsertActor(Actor);
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
	Partition.RemoveActor(Actor);
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
	Partition.UpdateActor(Actor);
}

FLODUpdateContext UWorld::PrepareLODContext()
{
	if (!ActiveCamera) return {};

	const FVector CameraPos = ActiveCamera->GetWorldLocation();
	const FVector CameraForward = ActiveCamera->GetForwardVector();

	const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
	const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
	const bool bShouldStaggerLOD = Scene.GetProxyCount() >= LOD_STAGGER_MIN_VISIBLE;

	const bool bForceFullLODRefresh =
		!bShouldStaggerLOD
		|| LastLODUpdateCamera != ActiveCamera
		|| !bHasLastFullLODUpdateCameraPos
		|| FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ
		|| CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

	if (bForceFullLODRefresh)
	{
		LastLODUpdateCamera = ActiveCamera;
		LastFullLODUpdateCameraPos = CameraPos;
		LastFullLODUpdateCameraForward = CameraForward;
		bHasLastFullLODUpdateCameraPos = true;
	}

	FLODUpdateContext Ctx;
	Ctx.CameraPos = CameraPos;
	Ctx.LODUpdateFrame = LODUpdateFrame;
	Ctx.LODUpdateSlice = LODUpdateSlice;
	Ctx.bForceFullRefresh = bForceFullLODRefresh;
	Ctx.bValid = true;
	return Ctx;
}

void UWorld::InitWorld()
{
	Partition.Reset(FBoundingBox());
	ClearLevels();
	
	PersistentLevel = UObjectManager::Get().CreateObject<ULevel>(this);
	PersistentLevel->SetWorld(this);
	Levels.push_back(PersistentLevel);
	CurrentLevel = PersistentLevel;
	
	AuthorGameMode = SpawnActor<AGameModeBase>();
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;
	for (ULevel* Level : Levels)
	{
		if (Level) Level->BeginPlay();
	}
	if (AuthorGameMode)
		AuthorGameMode->StartPlay();
}

void UWorld::Tick(float DeltaTime, ELevelTick TickType)
{
	{
		SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
		Partition.FlushPrimitive();
	}

	Scene.GetDebugDrawQueue().Tick(DeltaTime);

	TickManager.Tick(this, DeltaTime, TickType);
	UpdateOverlaps();
}

void UWorld::UpdateOverlaps() {
	const FOctree* Octree = GetOctree();
	if (!Octree) return;

	for (auto* Actor : GetActors()) {
		if (!Actor) continue;

		for (auto* Component : Actor->GetComponents()) {
			UShapeComponent* Shape = dynamic_cast<UShapeComponent*>(Component);
			if (!Shape || !Shape->IsCollisionEnabled() || !Shape->CanGenerateOverlapEvents()) continue;

			// End overlaps that are no longer valid
			TArray<FOverlapInfo> Prev = Shape->GetOverlapInfos();
			for (const FOverlapInfo& PrevInfo : Prev) {
				UShapeComponent* Other = dynamic_cast<UShapeComponent*>(PrevInfo.HitResult.Component);
				if (!Other || !FCollisionDispatcher::Get().CheckCollision(Shape, Other)) {
					Shape->EndComponentOverlap(PrevInfo.HitResult.Component);
					Shape->ShapeColor = FColor(0, 0, 255);
				}
			}

			// Broad phase
			TArray<UPrimitiveComponent*> Broad;
			Octree->QueryAABB(Shape->GetWorldBoundingBox(), Broad);

			// Narrow phase
			for (auto* Candidate : Broad) {
				if (!Candidate || Candidate == Shape) continue;
				UShapeComponent* Other = dynamic_cast<UShapeComponent*>(Candidate);
				if (!Other || !Other->IsCollisionEnabled() || !Other->CanGenerateOverlapEvents()) continue;

				FOverlapInfo Info;
				Info.HitResult.Component = Other;
				if (FCollisionDispatcher::Get().CheckCollision(Shape, Other, Info)) {
					Shape->BeginComponentOverlap(Info, true);
					Shape->ShapeColor = FColor(255, 0, 0);
				}
			}
		}
	}
}

void UWorld::EndPlay()
{
	bHasBegunPlay = false;
	TickManager.Reset();

	for (ULevel* Level : Levels)
	{
		if (Level) Level->EndPlay();
	}

	Partition.Reset(FBoundingBox());

	for (ULevel* Level : Levels)
	{
		if (Level) Level->Clear();
	}
	MarkWorldPrimitivePickingBVHDirty();

	ClearLevels();
}
