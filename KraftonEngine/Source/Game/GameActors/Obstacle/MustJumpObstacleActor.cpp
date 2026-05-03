#include "MustJumpObstacleActor.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(AMustJumpObstacleActor, AObstacleActorBase)

void AMustJumpObstacleActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh("Game.Mesh.Obstacle.MustJump0")) {
			MeshPath = Res->Path;
		}
	}
	Super::InitDefaultComponents(MeshPath);
}
