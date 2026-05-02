#include "Engine/Runtime/GameEngine.h"
#include "Core/ProjectSettings.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Platform/Paths.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/World.h"
#include <filesystem>

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

	std::filesystem::path ScenePath = std::filesystem::path(FSceneSaveManager::GetSceneDirectory())
		/ (FPaths::ToWide(StartLevel) + FSceneSaveManager::SceneExtension);
	FString FilePath = FPaths::ToUtf8(ScenePath.wstring());

	if (!std::filesystem::exists(ScenePath))
	{
		return;
	}

	FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle());
	if (!Context || !Context->World) return;

	FPerspectiveCameraData DummyCamera;
	if (FilePath.ends_with(".Scene") || FilePath.ends_with(".scene"))
	{
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, DummyCamera);
	}
	else if (FilePath.ends_with(".umap") || FilePath.ends_with(".UMAP"))
	{
		FSceneSaveManager::LoadWorldFromBinary(FilePath, Context->World);
	}
	
	if (Context->World)
	{
		Context->World->WarmupPickingData();
	}
}
