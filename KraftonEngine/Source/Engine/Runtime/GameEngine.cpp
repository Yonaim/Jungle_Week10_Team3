#include "Engine/Runtime/GameEngine.h"
#include "Core/ProjectSettings.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Platform/Paths.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/World.h"
#include <filesystem>
#include <iostream>

IMPLEMENT_CLASS(UGameEngine, UEngine)

void UGameEngine::Init(FWindowsWindow* InWindow)
{
	// 기본 엔진 초기화 (Renderer 등)
	UEngine::Init(InWindow);

	// 게임용 월드 컨텍스트 생성
	CreateWorldContext(EWorldType::Game, FName("GameWorld"));
	SetActiveWorld(FName("GameWorld"));
	GetWorld()->InitWorld();

	// 프로젝트 설정에서 시작 씬 로드
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

	// 우선순위: 쿠킹된 .umap → .Scene(JSON, dev/fallback)
	// Shipping에서는 .umap만 시도. JSON 경로는 dev 환경에서만 동작.
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
		std::cerr << "[GameEngine] Start level not found. Did you run --cook? Looked for "
		          << UmapPath.string() << std::endl;
		return;
	}

	const FString FilePath = FPaths::ToUtf8(ChosenPath.wstring());

	FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle());
	if (!Context || !Context->World) return;

	const FName OriginalHandle = Context->ContextHandle;
	const std::wstring ChosenExt = ChosenPath.extension().wstring();

	if (ChosenExt == L".umap" || ChosenExt == L".UMAP")
	{
		// 바이너리(쿠킹된) 경로 — 가장 안정적. World->Serialize가 자체 InitWorld+BeginPlay 처리.
		FSceneSaveManager::LoadWorldFromBinary(FilePath, Context->World);
	}
	else
	{
		// JSON 경로 — LoadSceneFromJSON이 새 World를 만들어 Context를 덮어씀
		FPerspectiveCameraData DummyCamera;
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, DummyCamera);
	}

	// 어느 경로든 WorldType/Handle을 Game으로 강제 복구
	Context->WorldType = EWorldType::Game;
	Context->ContextHandle = OriginalHandle;
	SetActiveWorld(OriginalHandle);

	if (Context->World)
	{
		Context->World->SetWorldType(EWorldType::Game);
		Context->World->WarmupPickingData();
	}
}
