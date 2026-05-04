#include "BarrierObstacleActor.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

IMPLEMENT_CLASS(ABarrierObstacleActor, AObstacleActorBase)

void ABarrierObstacleActor::InitDefaultComponents(const FString& UStaticMeshFileName) {
	FString MeshPath = UStaticMeshFileName;
	if (MeshPath.empty() || MeshPath == "None") {
		if (const FMeshResource* Res = FResourceManager::Get().FindMesh("Default.Mesh.BasicShape.Cube")) {
			MeshPath = Res->Path;
		}
	}
	Super::InitDefaultComponents(MeshPath);
}
