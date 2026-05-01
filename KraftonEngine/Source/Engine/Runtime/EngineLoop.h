#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsApplication.h"
#include "Engine/Profiling/Timer.h"
#include <mmsystem.h>

#pragma comment(lib, "Winmm.lib")
class FEngineLoop
{
public:
	bool Init(HINSTANCE hInstance, int nShowCmd);
	int Run();
	void Shutdown();

private:
	void CreateEngine();

private:
	FWindowsApplication Application;
	FTimer Timer;
};
