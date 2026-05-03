#include "ObstacleActorBase.h"  
#include "Component/Shape/BoxComponent.h"  
#include "Component/Shape/SphereComponent.h"  
#include "Component/Shape/CapsuleComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Runtime/Engine.h"

#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"

#include "Core/Log.h"

DEFINE_CLASS(AObstacleActorBase, AStaticMeshActor)

void AObstacleActorBase::BeginPlay() {
	Super::BeginPlay();
	for (UActorComponent* Comp : OwnedComponents)
	{
		if (UShapeComponent* Shape = Cast<UShapeComponent>(Comp))
		{
			Shape->OnComponentBeginOverlap.AddDynamic(this, &AObstacleActorBase::OnOverlap);
			Shape->OnComponentHit.AddDynamic(this, &AObstacleActorBase::OnHit);
		}
	}
}

void AObstacleActorBase::OnHit(const FComponentHitEvent& Other) {
	UE_LOG("Listening to a Hit Event, UUID: " + this->GetUUID());
	if (!Other.HitComponent) return;

}

void AObstacleActorBase::OnOverlap(const FComponentOverlapEvent& Other) {
	UE_LOG("Listening to an Overlap Event, UUID: " + this->GetUUID());
	if (!Other.OtherComponent) return;
}

void AObstacleActorBase::InitDefaultComponents(const FString& UStaticMeshFileName) {
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	StaticMeshComponent->SetCanDeleteFromDetails(false);
	SetRootComponent(StaticMeshComponent);
	UBoxComponent* CollisionBox = AddComponent<UBoxComponent>();
	CollisionBox->AttachToComponent(StaticMeshComponent);
	//CollisionBox->SetWorldLocation(StaticMeshComponent->GetWorldLocation());

	if (!UStaticMeshFileName.empty() && UStaticMeshFileName != "None")
	{
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		UStaticMesh* Asset = FObjManager::LoadObjStaticMesh(UStaticMeshFileName, Device);
		StaticMeshComponent->SetStaticMesh(Asset);

		if (Asset && IsBasicShapeAssetPath(UStaticMeshFileName))
		{
			const FString DefaultShapeMaterialPath = FResourceManager::Get().ResolvePath(FName("Default.Material.BasicShape"));
			if (UMaterial* DefaultShapeMaterial = FMaterialManager::Get().GetOrCreateMaterial(DefaultShapeMaterialPath))
			{
				int32 MaterialCount = static_cast<int32>(Asset->GetStaticMaterials().size());
				if (MaterialCount == 0 && Asset->GetStaticMeshAsset() &&
					(!Asset->GetStaticMeshAsset()->Sections.empty() || !Asset->GetStaticMeshAsset()->Indices.empty()))
				{
					MaterialCount = 1;
				}
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
				{
					StaticMeshComponent->SetMaterial(MaterialIndex, DefaultShapeMaterial);
				}
			}
		}
	}
	else
	{
		StaticMeshComponent->SetStaticMesh(nullptr);
	}
}
