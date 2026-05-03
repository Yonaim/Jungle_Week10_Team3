#include "MustSlideObstacleActor.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(AMustSlideObstacleActor, AObstacleActorBase)

void AMustSlideObstacleActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh("Game.Mesh.Obstacle.MustSlide0")) {
			MeshPath = Res->Path;
		}
	}
	Super::InitDefaultComponents(MeshPath);
}
