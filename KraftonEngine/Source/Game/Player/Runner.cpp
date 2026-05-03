#include "Runner.h"
#include "Object/Object.h"
#include "Component/Shape/BoxComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Resource/ResourceManager.h"
#include "Engine/Runtime/Engine.h"

#include "Materials/MaterialManager.h"
#include "Materials/Material.h"
#include "Mesh/ObjManager.h"
#include <Core/Log.h>
#include "Collision/OverlapInfo.h"


IMPLEMENT_CLASS(ARunner, APawnActor)

ARunner::ARunner()
{
	// PIE 또는 Shipping Build에서만 재생
	bNeedsTick = true;
	bTickInEditor = false;

	// RootComponent (Box Collision)
	BoxComponent = AddComponent<UBoxComponent>();
	BoxComponent->SetCanDeleteFromDetails(false);
	BoxComponent->SetBoxExtent(FVector(100.0f, 100.0f, 180.0f));
	BoxComponent->SetCollisionEnabled(true);
	
	SetRootComponent(BoxComponent);

	// Static Mesh
	MeshComponent = AddComponent<UStaticMeshComponent>();
	MeshComponent->SetCanDeleteFromDetails(false);
	MeshComponent->AttachToComponent(GetRootComponent());
	MeshComponent->SetRelativeScale(FVector(100.0f, 100.0f, 180.0f));
	MeshComponent->SetCollisionEnabled(false);

	// Lua Script
	PlayerMove = AddComponent<UScriptComponent>();
	PlayerMove->SetCanDeleteFromDetails(false);
	PlayerMove->SetScriptPath("Scripts/Game/PlayerController.lua");

	// Camera Component
	MainCamera = AddComponent<UCameraComponent>();
	MainCamera->SetCanDeleteFromDetails(false);
	MainCamera->SetRelativeLocation(FVector(-600.0f, 0.0f, 350.0f));
	MainCamera->AttachToComponent(GetRootComponent());
}

void ARunner::BeginPlay()
{
	// Lua BeginPlay에서 mesh local transform을 읽으므로 visual setup을 먼저 끝낸다.
	// 이 프로젝트의 Super::BeginPlay()가 ScriptComponent BeginPlay를 호출하기 때문에 일반 순서와 다르게 둔다.
	ApplyDefaultVisual();
	Super::BeginPlay();
	SetupCamera();
}

void ARunner::ApplyDefaultVisual()
{
	MeshComponent->SetRelativeScale(FVector(100.0f, 100.0f, 180.0f));

#pragma region Set Mesh & Material
	if (GEngine && MeshComponent)
	{
		// TODO: 일단 하드코딩 (추후 변경)
		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();

		FString MeshPath = "Asset/Content/Model/BasicShape/cube.obj";

		UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(MeshPath, Device);
		if (Mesh)
		{
			MeshComponent->SetStaticMesh(Mesh);
			MeshComponent->PostEditProperty("StaticMesh");
		}
		else
		{
			UE_LOG("[Runner] Failed to load mesh: %s", MeshPath.c_str());
		}

		FString MaterialPath = FResourceManager::Get().ResolvePath(
			FName("Default.Material.Wood"),
			"Asset/Content/Materials/Samples/Wood.mat"
		);

		UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
		if (LoadedMat)
		{
			const int32 SlotCount = MeshComponent->GetMaterialSlotCount();
			const int32 ApplyCount = SlotCount > 0 ? SlotCount : 1;

			for (int32 SlotIndex = 0; SlotIndex < ApplyCount; ++SlotIndex)
			{
				MeshComponent->SetMaterial(SlotIndex, LoadedMat);
				MeshComponent->MarkRenderStateDirty();
			}

			MeshComponent->PostEditProperty("StaticMesh");
		}
		else
		{
			UE_LOG("[Runner] Failed to load material: %s", MaterialPath.c_str());
		}
	}
#pragma endregion
}

void ARunner::SetupCamera()
{
	if (MainCamera)
	{
		MainCamera->SetRelativeLocation(FVector(-600.0f, 0.0f, 350.0f));

		// Runner 기준 앞쪽을 바라보게 한다.
		// X+가 전진 방향이므로 카메라는 Runner 뒤(-X)에 있고,
		// Runner 앞(+X)과 약간 위쪽을 바라본다.
		MainCamera->LookAt(GetActorLocation() + FVector(300.0f, 0.0f, 100.0f));
	}

	if (GetWorld() && MainCamera)
	{
		GetWorld()->SetActiveCamera(MainCamera);
	}
}

void ARunner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ARunner::EndPlay()
{
	Super::EndPlay();
}

void ARunner::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

UObject* ARunner::Duplicate(UObject* NewOuter /*= nullptr*/) const
{
	return Super::Duplicate(NewOuter);
}

void ARunner::TickActor(float DeltaSeconds, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaSeconds, TickType, ThisTickFunction);
}
