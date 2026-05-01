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
		if (Actor)
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