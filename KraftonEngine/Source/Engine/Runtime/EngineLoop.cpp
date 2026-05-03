#include "Engine/Runtime/EngineLoop.h"
#include "Core/ProjectSettings.h"
#include "Profiling/StartupProfiler.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include <iostream>

#if IS_OBJ_VIEWER
#include "ObjViewer/ObjViewerEngine.h"
#elif WITH_EDITOR
#include "Editor/EditorEngine.h"
#else
#include "Engine/Runtime/GameEngine.h"
#endif

void FEngineLoop::CreateEngine()
{
#if IS_OBJ_VIEWER
	GEngine = UObjectManager::Get().CreateObject<UObjViewerEngine>();
#elif WITH_EDITOR
	GEngine = UObjectManager::Get().CreateObject<UEditorEngine>();
#else
	GEngine = UObjectManager::Get().CreateObject<UGameEngine>();
#endif
}

bool FEngineLoop::Init(HINSTANCE hInstance, int nShowCmd)
{
	{
		SCOPE_STARTUP_STAT("WindowsApplication::Init");
		if (!Application.Init(hInstance))
		{
			return false;
		}
	}

	Application.SetOnSizingCallback([this]()
		{
			Timer.Tick();
			GEngine->Tick(Timer.GetDeltaTime());
		});

	Application.SetOnResizedCallback([](unsigned int Width, unsigned int Height)
		{
			if (GEngine)
			{
				GEngine->OnWindowResized(Width, Height);
			}
		});

	CreateEngine();

	{
		SCOPE_STARTUP_STAT("Engine::Init");
		GEngine->Init(&Application.GetWindow());
	}

	GEngine->SetTimer(&Timer);
	Timer.SetMaxFPS(
		FProjectSettings::Get().Performance.bLimitFPS
			? static_cast<float>(FProjectSettings::Get().Performance.MaxFPS)
			: 0.0f);

	{
		SCOPE_STARTUP_STAT("Engine::BeginPlay");
		GEngine->BeginPlay();
	}

	Timer.Initialize();
	FStartupProfiler::Get().Finish();

	return true;
}

int FEngineLoop::RunCookOnly()
{
	const int32 Cooked = FSceneSaveManager::CookAllScenes();
	std::cerr << "[Cook] Total cooked: " << Cooked << " scenes" << std::endl;
	return Cooked > 0 ? 0 : 1;
}

int FEngineLoop::Run()
{
	timeBeginPeriod(1);
	while (!Application.IsExitRequested())
	{
		Application.PumpMessages();

		if (Application.IsExitRequested())
		{
			break;
		}

		Timer.Tick();
		float DeltaTime = Timer.GetDeltaTime();
		GEngine->Tick(DeltaTime);

		const float MaxFPS = Timer.GetMaxFPS();
		const float TargetDeltaTime = MaxFPS > 0.0f ? (1.0f / MaxFPS) : 0.0f;
		float FrameProcessTime = Timer.GetTimeSinceLastTick();
		if (TargetDeltaTime > 0.0f && FrameProcessTime < TargetDeltaTime)
		{
			float RemainingTimeMS = (TargetDeltaTime - FrameProcessTime) * 1000.f;
			if (RemainingTimeMS > 2.0f)
			{
				Sleep(static_cast<DWORD>(RemainingTimeMS -1.f));
			}
			while (Timer.GetTimeSinceLastTick() < TargetDeltaTime)
			{
				_mm_pause();
			}
		}

	}
	timeEndPeriod(1);
	return 0;
}

void FEngineLoop::Shutdown()
{
	if (GEngine)
	{
		GEngine->Shutdown();
		UObjectManager::Get().DestroyObject(GEngine);
		GEngine = nullptr;
	}
}
