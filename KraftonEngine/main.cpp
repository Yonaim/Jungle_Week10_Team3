#include <Windows.h>
#include <string>
#include <iostream>
#include "Engine/Runtime/Launch.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Engine/Platform/Paths.h"
#include "Core/ProjectSettings.h"

namespace
{
	// 헤드리스 쿠킹 — 윈도우/렌더러를 띄우지 않고 .Scene → .umap만 변환.
	// FObjectFactory/FNamePool은 static init으로 자동 등록되므로 클래스 RTTI는 즉시 사용 가능.
	int RunCookCommandlet()
	{
		// ProjectSettings는 경로 결정에 필요 (현재는 직접적 의존은 없지만 미래 대비)
		FProjectSettings::Get().LoadFromFile(FProjectSettings::GetDefaultPath());

		const int32 Cooked = FSceneSaveManager::CookAllScenes();
		std::cerr << "[Cook] Total cooked: " << Cooked << " scenes" << std::endl;
		return Cooked > 0 ? 0 : 1;
	}

	bool HasCommandLineFlag(const char* Flag)
	{
		LPSTR CmdLine = GetCommandLineA();
		if (!CmdLine) return false;
		const std::string CmdLineStr = CmdLine;
		return CmdLineStr.find(Flag) != std::string::npos;
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nShowCmd)
{
	// 쿠킹 모드: 엔진 윈도우/렌더러 초기화 없이 씬 변환만 수행하고 종료
	if (HasCommandLineFlag("--cook") || HasCommandLineFlag("-cook"))
	{
		return RunCookCommandlet();
	}

	return Launch(hInstance, nShowCmd);
}
