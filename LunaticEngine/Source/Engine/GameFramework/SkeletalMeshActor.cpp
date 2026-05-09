#include "GameFramework/SkeletalMeshActor.h"
#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Component/SkeletalMeshComponent.h"
#include "Mesh/ObjManager.h"

IMPLEMENT_CLASS(ASkeletalMeshActor, AActor)

void ASkeletalMeshActor::InitDefaultComponents(const FString& FbxFileName)
{
	SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
	SkeletalMeshComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(SkeletalMeshComponent);

	if (!FbxFileName.empty() && FbxFileName != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		USkeletalMesh* Asset = FObjManager::LoadObjSkeletalMesh(FbxFileName, Device);
		SkeletalMeshComponent->SetSkeletalMesh(Asset);
	}
	else
	{
		SkeletalMeshComponent->SetSkeletalMesh(nullptr);
	}
}
