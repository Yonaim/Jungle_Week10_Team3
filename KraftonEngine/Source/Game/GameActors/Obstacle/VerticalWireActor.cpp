#include "VerticalWireActor.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(AVerticalWireActor, AObstacleActorBase)

void AVerticalWireActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh("Game.Mesh.Obstacle.VerticalWire")) {
			MeshPath = Res->Path;
		}
	}
	Super::InitDefaultComponents(MeshPath);
}
