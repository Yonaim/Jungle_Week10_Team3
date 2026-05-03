#include "Engine/Runtime/GameEngine.h"
#include "Core/ProjectSettings.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Platform/Paths.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/CameraComponent.h"
#include "Component/ActorComponent.h"
#include "Input/InputManager.h"
#include "Viewport/GameViewportClient.h"
#include "Engine/Runtime/WindowsWindow.h"
#include <filesystem>

IMPLEMENT_CLASS(UGameEngine, UEngine)

void UGameEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);

	// Shipping/Game에서는 ImGui 컨텍스트가 없으므로 InputManager가 ImGui::GetIO()를 호출하지 않도록
	// GUI capture를 명시적으로 false override 한다 (ImGui 미초기화 시 GetIO() 사용은 크래시).
	FInputManager::Get().SetGuiCaptureOverride(false, false, false);

	CreateWorldContext(EWorldType::Game, FName("GameWorld"));
	SetActiveWorld(FName("GameWorld"));
	GetWorld()->InitWorld();

	UGameViewportClient* ViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
	SetGameViewportClient(ViewportClient);
	if (InWindow)
	{
		ViewportClient->SetOwnerWindow(InWindow->GetHWND());
	}

	LoadStartLevel();

	if (FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle()))
	{
		ViewportClient->OnBeginPIE(Context->World ? Context->World->GetActiveCamera() : nullptr, nullptr);
		ViewportClient->SetPIEPossessedInputEnabled(true);
	}
}

void UGameEngine::BeginPlay()
{
	UEngine::BeginPlay();
}

void UGameEngine::Tick(float DeltaTime)
{
	if (UGameViewportClient* GameVC = GetGameViewportClient())
	{
		GameVC->ProcessPIEInput(DeltaTime);
		GameVC->Tick(DeltaTime);
	}
	UEngine::Tick(DeltaTime);
}

void UGameEngine::LoadStartLevel()
{
	const FString& StartLevel = FProjectSettings::Get().Game.DefaultScene;
	if (StartLevel.empty())
	{
		return;
	}

	const std::filesystem::path SceneDir = FSceneSaveManager::GetSceneDirectory();
	const std::wstring StemW = FPaths::ToWide(StartLevel);

	// 우선순위: 쿠킹된 .umap → .Scene(JSON, dev/fallback). Shipping에서는 .umap 전용.
	const std::filesystem::path UmapPath = SceneDir / (StemW + L".umap");
	const std::filesystem::path ScenePath = SceneDir / (StemW + FSceneSaveManager::SceneExtension);

	std::filesystem::path ChosenPath;
	if (std::filesystem::exists(UmapPath))
	{
		ChosenPath = UmapPath;
	}
#if !defined(SHIPPING) || SHIPPING == 0
	else if (std::filesystem::exists(ScenePath))
	{
		ChosenPath = ScenePath;
	}
#endif

	if (ChosenPath.empty())
	{
		return;
	}

	const FString FilePath = FPaths::ToUtf8(ChosenPath.wstring());

	FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle());
	if (!Context || !Context->World)
	{
		return;
	}

	const FName OriginalHandle = Context->ContextHandle;

	FPerspectiveCameraData CamData;
	FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, CamData);

	// LoadSceneFromJSON이 새 World를 만들어 Context를 덮어쓰고, JSON에서 읽은 WorldType("Editor")을 Game으로 강제 복구
	Context->WorldType = EWorldType::Game;
	Context->ContextHandle = OriginalHandle;
	SetActiveWorld(OriginalHandle);

	if (Context->World)
	{
		Context->World->SetWorldType(EWorldType::Game);
		Context->World->WarmupPickingData();

		// Game/Shipping에서는 에디터 뷰포트가 없으므로 씬 안의 첫 카메라 컴포넌트를 ActiveCamera로 잡음.
		if (!Context->World->GetActiveCamera())
		{
			for (AActor* Actor : Context->World->GetActors())
			{
				if (!Actor) continue;
				for (UActorComponent* Comp : Actor->GetComponents())
				{
					if (UCameraComponent* Cam = Cast<UCameraComponent>(Comp))
					{
						Context->World->SetActiveCamera(Cam);
						break;
					}
				}
				if (Context->World->GetActiveCamera()) break;
			}
		}

		// 씬 어디에도 UCameraComponent가 없으면(에디터에서 PerspectiveCamera만 저장된 일반 .Scene)
		// 저장된 에디터 뷰포트 좌표를 사용해 기본 카메라 액터를 한 개 스폰한다.
		if (!Context->World->GetActiveCamera())
		{
			AActor* CamActor = Context->World->SpawnActor<AActor>();
			if (CamActor)
			{
				CamActor->SetFName(FName("DefaultGameCamera"));
				UCameraComponent* Cam = CamActor->AddComponent<UCameraComponent>();
				CamActor->SetRootComponent(Cam);
				if (CamData.bValid)
				{
					Cam->SetRelativeLocation(CamData.Location);
					Cam->SetRelativeRotation(CamData.Rotation);
				}
				else
				{
					// fallback: 위에서 비스듬히 바라보기
					Cam->SetRelativeLocation(FVector(0.0f, -10.0f, 5.0f));
					Cam->SetRelativeRotation(FVector(0.0f, -25.0f, 90.0f));
				}
				Context->World->SetActiveCamera(Cam);
			}
		}
	}
}
