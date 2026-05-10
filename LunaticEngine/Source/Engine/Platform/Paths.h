#pragma once

#include <string>
#include <Windows.h>

// 엔진 전역 경로를 관리합니다.
// 모든 경로는 실행 파일 기준 상대 경로이며, 한글 경로를 위해 wstring 기반입니다.
class FPaths
{
public:
	// 프로젝트 루트 (실행 파일이 있는 디렉터리)
	static std::wstring RootDir();

	// 주요 디렉터리
	static std::wstring ShaderDir();      // Shaders/
	static std::wstring AssetDir();       // Asset/
	static std::wstring ContentDir();     // Asset/Content/
	static std::wstring SceneDir();       // Asset/Content/Scene/
	static std::wstring DataDir();        // Legacy alias of Asset/Content/
	static std::wstring SaveDir();        // Saves/
	static std::wstring DumpDir();        // Saves/Dump/
	static std::wstring LogDir();         // Saves/Logs/
	static std::wstring SettingsDir();    // Settings/
	static std::wstring ScriptsDir();     // Scripts/

	// 주요 파일 경로
	static std::wstring SettingsFilePath();         // Settings/Editor.ini
	static std::wstring ResourceFilePath();         // Legacy alias
	static std::wstring ResourceSettingsDir();      // Settings/Resource/
	static std::wstring EditorResourceFilePath();   // Settings/Resource/EditorResources.ini
	static std::wstring DefaultContentResourceFilePath(); // Settings/Resource/DefaultContentResources.ini
	static std::wstring ProjectResourcePathsFilePath();  // Settings/Resource/ProjectResourcePaths.ini
	static std::wstring ProjectSettingsFilePath();  // Settings/ProjectSettings.ini

	static std::wstring ProjectDir();
	static std::wstring ProjectContentDir();
	static std::wstring ProjectConfigDir();
	static std::wstring ProjectSavedDir();

	// Path Utilities
	static std::string ConvertRelativePathToFull(const std::string& RelativePath);
	static std::string NormalizePath(const std::string& Path);

	//  FPaths::Combine(L"Asset/Content/Scene", L"Default.Scene")
	static std::wstring Combine(const std::wstring& Base, const std::wstring& Child);


	static void CreateDir(const std::wstring& Path);


	static std::wstring ToWide(const std::string& Utf8Str);
	static std::string ToUtf8(const std::wstring& WideStr);

	static std::string ResolveAssetPath(const std::string& BaseFilePath, const std::string& TargetPath);
};
