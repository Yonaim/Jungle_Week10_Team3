#include "Engine/Runtime/EngineLoop.h"
#include "Profiling/StartupProfiler.h"

#if IS_OBJ_VIEWER
#include "ObjViewer/ObjViewerEngine.h"
#elif WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

void FEngineLoop::CreateEngine()
{
#if IS_OBJ_VIEWER
	GEngine = UObjectManager::Get().CreateObject<UObjViewerEngine>();
#elif WITH_EDITOR
	GEngine = UObjectManager::Get().CreateObject<UEditorEngine>();
#else
	GEngine = UObjectManager::Get().CreateObject<UEngine>();
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

	{
		SCOPE_STARTUP_STAT("Engine::BeginPlay");
		GEngine->BeginPlay();
	}

	Timer.Initialize();
	FStartupProfiler::Get().Finish();

	return true;
}

int FEngineLoop::Run()
{
	timeBeginPeriod(1);
	const float TargetDeltaTime = 1.f / 60.f;
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

		float FrameProcessTime = Timer.GetTimeSinceLastTick();
		if (FrameProcessTime < TargetDeltaTime)
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
