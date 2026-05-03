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

	LoadStartLevel();
}

void UGameEngine::BeginPlay()
{
	UEngine::BeginPlay();
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

	FPerspectiveCameraData DummyCamera;
	FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, DummyCamera);

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
	}
}
