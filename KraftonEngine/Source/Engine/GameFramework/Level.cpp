#include "GameFramework/Level.h"
#include "Object/ObjectFactory.h"
#include <GameFramework/World.h>
#include "Serialization/Archive.h"
IMPLEMENT_CLASS(ULevel, UObject)

ULevel::ULevel(UWorld* OwingWorld)
	: OwingWorld(OwingWorld)
{
	Actors.clear();
}

ULevel::ULevel(const TArray<AActor*>& Actors, UWorld* World)
	: Actors(Actors)
{
	OwingWorld = World;
}

ULevel::~ULevel()
{
	Clear();
	OwingWorld = nullptr;
}

void ULevel::AddActor(AActor* Actor)
{
	if (!Actor) return;

	auto It = std::find(Actors.begin(), Actors.end(), Actor);
	if (It != Actors.end())
	{
		return;
	}
	
	Actor->SetOuter(this);
	Actors.push_back(Actor);
}

void ULevel::RemoveActor(AActor* Actor)
{
	if (!Actor) return;

	auto It = std::find(Actors.begin(), Actors.end(), Actor);
	if (It == Actors.end())
	{
		return;
	}

	Actors.erase(It);
}

bool ULevel::MoveActorBefore(AActor* ActorToMove, AActor* BeforeActor)
{
	if (!ActorToMove || !BeforeActor || ActorToMove == BeforeActor)
	{
		return false;
	}

	auto MoveIt = std::find(Actors.begin(), Actors.end(), ActorToMove);
	auto BeforeIt = std::find(Actors.begin(), Actors.end(), BeforeActor);
	if (MoveIt == Actors.end() || BeforeIt == Actors.end())
	{
		return false;
	}

	AActor* MovedActor = *MoveIt;
	Actors.erase(MoveIt);
	BeforeIt = std::find(Actors.begin(), Actors.end(), BeforeActor);
	Actors.insert(BeforeIt, MovedActor);
	return true;
}

bool ULevel::MoveActorToIndex(AActor* ActorToMove, size_t TargetIndex)
{
	if (!ActorToMove)
	{
		return false;
	}

	auto MoveIt = std::find(Actors.begin(), Actors.end(), ActorToMove);
	if (MoveIt == Actors.end())
	{
		return false;
	}

	AActor* MovedActor = *MoveIt;
	Actors.erase(MoveIt);

	if (TargetIndex > Actors.size())
	{
		TargetIndex = Actors.size();
	}

	Actors.insert(Actors.begin() + static_cast<std::ptrdiff_t>(TargetIndex), MovedActor);
	return true;
}

void ULevel::Clear()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->SetOuter(nullptr);
		}
	}

	Actors.clear();
}

void ULevel::Tick(float DeltaTime) {
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void ULevel::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsSaving())
	{
		Ar << GameModeClassName;

		int32 ActorCount = static_cast<int32>(Actors.size());
		Ar << ActorCount;

		for (AActor* actor : Actors)
		{
			FString ClassName = actor->GetClass()->GetName();
			Ar << ClassName;
			actor->Serialize(Ar);
		}

	}
	else if (Ar.IsLoading())
	{
		Ar << GameModeClassName;

		int32 ActorCount = 0;
		Ar << ActorCount;

		for (int i = 0; i < ActorCount; ++i)
		{
			FString ClassName;
			Ar << ClassName;

			UObject* Obj = FObjectFactory::Get().Create(ClassName, this);
			AActor * NewActor = Cast<AActor>(Obj);

			if (NewActor)
			{
				NewActor->Serialize(Ar);
				AddActor(NewActor);
			}
		}
	}
}

void ULevel::BeginPlay()
{
	const size_t InitialCount = Actors.size();
	for (size_t i = 0; i < InitialCount; ++i)
	{
		AActor* Actor = Actors[i];
		if (Actor && !Actor->HasActorBegunPlay())
		{
			Actor->BeginPlay();
		}
	}
}

void ULevel::EndPlay()
{
	for (AActor* Actor : Actors)
	{
		if (Actor && IsAliveObject(Actor))
		{
			Actor->EndPlay();
		}
	}

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			UObjectManager::Get().DestroyObject(Actor);
		}
	}
	Actors.clear();
}
