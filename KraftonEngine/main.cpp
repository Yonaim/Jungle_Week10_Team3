#include <Windows.h>
#include <string>
#include "Engine/Runtime/Launch.h"
#include "Engine/Runtime/EngineLoop.h"

namespace
{
	bool HasCommandLineFlag(const char* Flag)
	{
		LPSTR CmdLine = GetCommandLineA();
		if (!CmdLine) return false;
		const std::string CmdLineStr = CmdLine;
		return CmdLineStr.find(Flag) != std::string::npos;
	}


	int RunCookCommandlet(HINSTANCE hInstance, int nShowCmd)
	{
		FEngineLoop EngineLoop;
		if (!EngineLoop.Init(hInstance, nShowCmd))
		{
			return -1;
		}
		const int Result = EngineLoop.RunCookOnly();
		EngineLoop.Shutdown();
		return Result;
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nShowCmd)
{
	if (HasCommandLineFlag("--cook") || HasCommandLineFlag("-cook"))
	{
		return RunCookCommandlet(hInstance, nShowCmd);
	}

	return Launch(hInstance, nShowCmd);
}
