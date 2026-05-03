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
#include "Object/Object.h"
#include <cctype>
#include <filesystem>

IMPLEMENT_CLASS(UGameEngine, UEngine)

namespace
{
	bool EndsWithIgnoreCase(const FString& Value, const char* Suffix)
	{
		if (!Suffix)
		{
			return false;
		}

		const FString SuffixString = Suffix;
		if (Value.size() < SuffixString.size())
		{
			return false;
		}

		for (size_t Index = 0; Index < SuffixString.size(); ++Index)
		{
			const char Left = static_cast<char>(std::tolower(static_cast<unsigned char>(Value[Value.size() - SuffixString.size() + Index])));
			const char Right = static_cast<char>(std::tolower(static_cast<unsigned char>(SuffixString[Index])));
			if (Left != Right)
			{
				return false;
			}
		}

		return true;
	}
}

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

	if (LoadScene(StartLevel))
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

bool UGameEngine::LoadScene(const FString& InSceneReference)
{
	if (InSceneReference.empty())
	{
		return false;
	}

	std::filesystem::path ChosenPath;
	const std::filesystem::path RawPath = FPaths::ToWide(InSceneReference);
	const std::filesystem::path SceneDir = FSceneSaveManager::GetSceneDirectory();

	auto TrySetChosenPath = [&ChosenPath](const std::filesystem::path& Candidate)
	{
		if (!Candidate.empty() && std::filesystem::exists(Candidate))
		{
			ChosenPath = Candidate;
			return true;
		}
		return false;
	};

	if (RawPath.is_absolute())
	{
		TrySetChosenPath(RawPath);
	}
	else
	{
		TrySetChosenPath(RawPath);
		if (ChosenPath.empty())
		{
			TrySetChosenPath(SceneDir / RawPath);
		}
	}

	if (ChosenPath.empty())
	{
		const bool bHasSceneExtension = EndsWithIgnoreCase(InSceneReference, ".scene");
		const bool bHasUmapExtension = EndsWithIgnoreCase(InSceneReference, ".umap");
		if (bHasSceneExtension || bHasUmapExtension)
		{
			const std::filesystem::path FileName = RawPath.filename();
			if (!TrySetChosenPath(SceneDir / FileName))
			{
				return false;
			}
		}
		else
		{
			const std::wstring StemW = FPaths::ToWide(InSceneReference);
			if (!TrySetChosenPath(SceneDir / (StemW + L".umap")))
			{
				if (!TrySetChosenPath(SceneDir / (StemW + FSceneSaveManager::SceneExtension)))
				{
					return false;
				}
			}
		}
	}

	FWorldContext* Context = GetWorldContextFromHandle(GetActiveWorldHandle());
	if (!Context)
	{
		return false;
	}

	if (Context->World)
	{
		Context->World->EndPlay();
		UObjectManager::Get().DestroyObject(Context->World);
		Context->World = nullptr;
	}

	FPerspectiveCameraData DummyCamera;
	const FString FilePath = FPaths::ToUtf8(ChosenPath.wstring());
	if (EndsWithIgnoreCase(FilePath, ".umap"))
	{
		Context->World = UObjectManager::Get().CreateObject<UWorld>();
		FSceneSaveManager::LoadWorldFromBinary(FilePath, Context->World);
		Context->WorldType = EWorldType::Game;
		Context->ContextName = RawPath.stem().empty() ? "GameWorld" : FPaths::ToUtf8(RawPath.stem().wstring());
		Context->ContextHandle = GetActiveWorldHandle();
	}
	else
	{
		FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, DummyCamera);
		Context->WorldType = EWorldType::Game;
		Context->ContextHandle = GetActiveWorldHandle();
	}

	SetActiveWorld(Context->ContextHandle);

	if (!Context->World)
	{
		return false;
	}

	Context->World->SetWorldType(EWorldType::Game);
	Context->World->WarmupPickingData();
	if (!Context->World->HasBegunPlay())
	{
		Context->World->BeginPlay();
	}
	return true;
}
