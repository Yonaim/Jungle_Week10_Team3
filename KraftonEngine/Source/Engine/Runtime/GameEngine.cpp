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

	// LoadSceneFromJSON이 새 World를 만들어 Context를 덮어씀 — 기존 World/Handle을 백업해두고 끝나면 복구.
	const FName OriginalHandle = Context->ContextHandle;

	FPerspectiveCameraData DummyCamera;
	if (FilePath.ends_with(".Scene") || FilePath.ends_with(".scene"))
	{
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, DummyCamera);
	}
	else if (FilePath.ends_with(".umap") || FilePath.ends_with(".UMAP"))
	{
		FSceneSaveManager::LoadWorldFromBinary(FilePath, Context->World);
	}

	// JSON 저장본의 WorldType("Editor")을 Game으로 강제 복구 — UEngine::BeginPlay가 World->BeginPlay를 호출하려면 필수.
	Context->WorldType = EWorldType::Game;
	Context->ContextHandle = OriginalHandle;
	SetActiveWorld(OriginalHandle);

	// 주의: 기존 World 객체는 의도적으로 destroy하지 않음.
	// 렌더 프록시 등 외부 참조가 남아있을 수 있어 즉시 destroy 시 크래시 위험.
	// 엔진 셧다운 시 UObjectManager가 일괄 정리하도록 둠 (작은 1회성 누수).

	if (Context->World)
	{
		Context->World->SetWorldType(EWorldType::Game);
		Context->World->WarmupPickingData();
	}
}
