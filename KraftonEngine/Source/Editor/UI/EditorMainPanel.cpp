#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Component/CameraComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Pipeline/Renderer.h"
#include "Engine/Input/InputManager.h"

#include "Editor/UI/ImGuiSetting.h"
#include "Editor/UI/NotificationToast.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Editor/UI/EditorFileUtils.h"

namespace
{
	void ApplyEditorTabStyle()
	{
		ImGuiStyle& Style = ImGui::GetStyle();
		Style.TabRounding = (std::max)(Style.TabRounding, 6.0f);
		Style.TabBorderSize = (std::max)(Style.TabBorderSize, 1.0f);

		Style.Colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
		Style.Colors[ImGuiCol_TabHovered] = ImVec4(0.27f, 0.27f, 0.31f, 1.0f);
		Style.Colors[ImGuiCol_TabSelected] = ImVec4(0.24f, 0.24f, 0.27f, 1.0f);
		Style.Colors[ImGuiCol_TabDimmed] = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);
		Style.Colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.22f, 0.22f, 0.25f, 1.0f);
	}
}

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiSetting::LoadSetting();

	ImGuiIO& IO = ImGui::GetIO();
	IO.IniFilename = "Settings/imgui.ini";
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;
	ApplyEditorTabStyle();

	Window = InWindow;
	EditorEngine = InEditorEngine;

	ImGuiStyle& Style = ImGui::GetStyle();
	Style.WindowPadding.x = (std::max)(Style.WindowPadding.x, 12.0f);
	Style.WindowPadding.y = (std::max)(Style.WindowPadding.y, 10.0f);
	Style.FramePadding.x = (std::max)(Style.FramePadding.x, 8.0f);
	Style.FramePadding.y = (std::max)(Style.FramePadding.y, 6.0f);
	Style.ItemSpacing.x = (std::max)(Style.ItemSpacing.x, 10.0f);
	Style.ItemSpacing.y = (std::max)(Style.ItemSpacing.y, 8.0f);
	Style.CellPadding.x = (std::max)(Style.CellPadding.x, 8.0f);
	Style.CellPadding.y = (std::max)(Style.CellPadding.y, 6.0f);

	const FString FontPath = FResourceManager::Get().ResolvePath(FName("Default.Font.UI"));
	IO.Fonts->AddFontFromFileTTF(FontPath.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	DetailsWidget.Initialize(InEditorEngine);
	OutlinerWidget.Initialize(InEditorEngine);
	PlaceActorsWidget.Initialize(InEditorEngine);
	StatWidget.Initialize(InEditorEngine);
	ContentBrowserWidget.Initialize(InEditorEngine, InRenderer.GetFD3DDevice().GetDevice());
	ShadowMapDebugWidget.Initialize(InEditorEngine);
}

void FEditorMainPanel::Release()
{
	ConsoleWidget.Shutdown();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::SaveToSettings() const
{
	ContentBrowserWidget.SaveToSettings();
}

void FEditorMainPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiWindowClass DockspaceWindowClass{};
	DockspaceWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None, &DockspaceWindowClass);
	RenderMainMenuBar();

	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
	if (EditorEngine)
	{
		SCOPE_STAT_CAT("EditorEngine->RenderViewportUI", "5_UI");
		EditorEngine->RenderViewportUI(DeltaTime);

		if (FLevelEditorViewportClient* ActiveViewport = EditorEngine->GetActiveViewport())
		{
			EditorEngine->GetOverlayStatSystem().RenderImGui(*EditorEngine, ActiveViewport->GetViewportScreenRect());
		}
	}

	const FEditorSettings& Settings = FEditorSettings::Get();

	if (!bHideEditorWindows && Settings.UI.bImGUISettings)
	{
		ImGuiSetting::ShowSetting();
	}

	if (!bHideEditorWindows && Settings.UI.bConsole)
	{
		SCOPE_STAT_CAT("ConsoleWidget.Render", "5_UI");
		ConsoleWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bProperty)
	{
		SCOPE_STAT_CAT("DetailsWidget.Render", "5_UI");
		DetailsWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bScene)
	{
		SCOPE_STAT_CAT("OutlinerWidget.Render", "5_UI");
		OutlinerWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bPlaceActors)
	{
		SCOPE_STAT_CAT("PlaceActorsWidget.Render", "5_UI");
		PlaceActorsWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bStat)
	{
		SCOPE_STAT_CAT("StatWidget.Render", "5_UI");
		StatWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bContentBrowser)
	{
		SCOPE_STAT_CAT("ContentBrowserWidget.Render", "5_UI");
		ContentBrowserWidget.Render(DeltaTime);
	}

	if (!bHideEditorWindows && Settings.UI.bShadowMapDebug)
	{
		ShadowMapDebugWidget.Render(DeltaTime);
	}

	RenderShortcutOverlay();

	// 토스트 알림 (항상 최상위에 표시)
	FNotificationToast::Render();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::RenderMainMenuBar()
{
	if (!ImGui::BeginMainMenuBar())
	{
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New Scene", "Ctrl+N") && EditorEngine)
		{
			EditorEngine->NewScene();
		}
		if (ImGui::MenuItem("Open Scene...", "Ctrl+O") && EditorEngine)
		{
			EditorEngine->LoadSceneWithDialog();
		}
		if (ImGui::MenuItem("Save Scene", "Ctrl+S") && EditorEngine)
		{
			EditorEngine->SaveScene();
		}
		if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S") && EditorEngine)
		{
			EditorEngine->SaveSceneAsWithDialog();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Import Material...") && EditorEngine)
		{
			EditorEngine->ImportMaterialWithDialog();
		}
		if (ImGui::MenuItem("Import Texture...") && EditorEngine)
		{
			EditorEngine->ImportTextureWithDialog();
		}

		ImGui::Separator();
		const char* CurrentSceneLabel = "Current: Unsaved Scene";
		FString CurrentScenePath;
		FString CurrentSceneText;
		if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
		{
			CurrentScenePath = EditorEngine->GetCurrentLevelFilePath();
			CurrentSceneText = FString("Current: ") + CurrentScenePath;
			CurrentSceneLabel = CurrentSceneText.c_str();
		}
		ImGui::BeginDisabled();
		ImGui::MenuItem(CurrentSceneLabel, nullptr, false, false);
		ImGui::EndDisabled();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Levels"))
	{
		UWorld* World = EditorEngine->GetWorld();
		if (World)
		{
			// Persistent Level info
			ULevel* Persistent = World->GetPersistentLevel();
			FString PersistentName = Persistent ? "Persistent Level" : "No Persistent Level";
			bool bIsPersistentCurrent = (World->GetCurrentLevel() == Persistent);
			if (ImGui::MenuItem(PersistentName.c_str(), nullptr, bIsPersistentCurrent))
			{
				World->SetCurrentLevel(Persistent);
			}

			ImGui::Separator();
			ImGui::TextDisabled("Streaming Levels");

			for (const auto& Info : World->GetStreamingLevels())
			{
				bool bIsCurrent = (World->GetCurrentLevel() == Info.LoadedLevel);
				FString DisplayName = Info.LevelName.ToString() + (Info.bIsLoaded ? "" : " (Unloaded)");

				if (ImGui::MenuItem(DisplayName.c_str(), nullptr, bIsCurrent))
				{
					if (Info.LoadedLevel) World->SetCurrentLevel(Info.LoadedLevel);
				}

				if (ImGui::BeginPopupContextItem())
				{
					if (!Info.bIsLoaded)
					{
						if (ImGui::MenuItem("Load Level")) World->LoadStreamingLevel(Info.LevelPath);
					}
					else
					{
						if (ImGui::MenuItem("Unload Level")) World->UnloadStreamingLevel(Info.LevelName);
					}
					ImGui::EndPopup();
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Add Existing Level..."))
			{
				const std::wstring InitialDir = FSceneSaveManager::GetSceneDirectory();
				const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
					.Filter = L"Level Files (*.umap)\0*.umap\0",
					.Title = L"Add Existing Level",
					.InitialDirectory = InitialDir.c_str(),
					});
				if (!SelectedPath.empty())
				{
					World->AddStreamingLevel(SelectedPath);
				}
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::MenuItem("Windows"))
	{
		bShowWidgetList = true;
		ImGui::OpenPopup("##WidgetListPopup");
	}
	if (ImGui::BeginPopup("##WidgetListPopup"))
	{
		ImGui::Checkbox("Console", &Settings.UI.bConsole);
		ImGui::Checkbox("Details", &Settings.UI.bProperty);
		ImGui::Checkbox("Outliner", &Settings.UI.bScene);
		ImGui::Checkbox("Place Actors", &Settings.UI.bPlaceActors);
		ImGui::Checkbox("Stat Profiler", &Settings.UI.bStat);
		ImGui::Checkbox("Content Browser", &Settings.UI.bContentBrowser);
		ImGui::Checkbox("Shadow Map Debug", &Settings.UI.bShadowMapDebug);
		ImGui::Separator();
		ImGui::Checkbox("ImGuiSetting", &Settings.UI.bImGUISettings);
		ImGui::EndPopup();
	}
	else
	{
		bShowWidgetList = false;
	}

	if (ImGui::MenuItem("Shortcut"))
	{
		bShowShortcutOverlay = !bShowShortcutOverlay;
	}

	ImGui::EndMainMenuBar();
}

void FEditorMainPanel::RenderShortcutOverlay()
{
	if (!bShowShortcutOverlay)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(320.0f, 150.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Shortcut Help", &bShowShortcutOverlay, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("File");
	ImGui::Separator();
	ImGui::TextUnformatted("Ctrl+N : New Scene");
	ImGui::TextUnformatted("Ctrl+O : Open Scene");
	ImGui::TextUnformatted("Ctrl+S : Save Scene");
	ImGui::TextUnformatted("Ctrl+Shift+S : Save Scene As");
	ImGui::Separator();
	ImGui::TextUnformatted("F : Focus on selection");
	ImGui::TextUnformatted("Ctrl + LMB : Multi Picking (Toggle)");
	ImGui::TextUnformatted("Ctrl + Alt + LMB Drag : Area Selection");

	ImGui::End();
}

void FEditorMainPanel::Update()
{
	HandleGlobalShortcuts();

	ImGuiIO& IO = ImGui::GetIO();

	// 뷰포트 슬롯 위에서는 bUsingMouse를 해제해야 TickInteraction이 동작
	bool bWantMouse = IO.WantCaptureMouse;
	bool bWantKeyboard = IO.WantCaptureKeyboard || bShowShortcutOverlay;


	// IME는 ImGui가 텍스트 입력을 원할 때만 활성화.
	if (Window)
	{
		HWND hWnd = Window->GetHWND();
		if (IO.WantTextInput)
		{
			ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
		}
		else
		{
			ImmAssociateContext(hWnd, NULL);
		}
	}
}

void FEditorMainPanel::HandleGlobalShortcuts()
{
	if (!EditorEngine)
	{
		return;
	}
	if (EditorEngine->IsPIEPossessedMode())
	{
		return;
	}

	ImGuiIO& IO = ImGui::GetIO();
	if (IO.WantTextInput)
	{
		return;
	}

	FInputManager& Input = FInputManager::Get();
	if (!Input.IsKeyDown(VK_CONTROL))
	{
		return;
	}

	const bool bShift = Input.IsKeyDown(VK_SHIFT);
	if (Input.IsKeyPressed('N'))
	{
		EditorEngine->NewScene();
	}
	else if (Input.IsKeyPressed('O'))
	{
		EditorEngine->LoadSceneWithDialog();
	}
	else if (Input.IsKeyPressed('S'))
	{
		if (bShift)
		{
			EditorEngine->SaveSceneAsWithDialog();
		}
		else
		{
			EditorEngine->SaveScene();
		}
	}
}

void FEditorMainPanel::HideEditorWindows()
{
	if (bHasSavedUIVisibility)
	{
		bHideEditorWindows = true;
		bShowWidgetList = false;
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	SavedUIVisibility = Settings.UI;
	bSavedShowWidgetList = bShowWidgetList;
	bHasSavedUIVisibility = true;
	bHideEditorWindows = true;
	bShowWidgetList = false;

	Settings.UI.bConsole = false;
	Settings.UI.bProperty = false;
	Settings.UI.bScene = false;
	Settings.UI.bPlaceActors = false;
	Settings.UI.bStat = false;
	Settings.UI.bContentBrowser = false;
	Settings.UI.bImGUISettings = false;
	Settings.UI.bShadowMapDebug = false;
}

void FEditorMainPanel::ShowEditorWindows()
{
	if (!bHasSavedUIVisibility)
	{
		bHideEditorWindows = false;
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	Settings.UI = SavedUIVisibility;
	bShowWidgetList = bSavedShowWidgetList;
	bHideEditorWindows = false;
	bHasSavedUIVisibility = false;
}

void FEditorMainPanel::HideEditorWindowsForPIE()
{
	HideEditorWindows();
}

void FEditorMainPanel::RestoreEditorWindowsAfterPIE()
{
	ShowEditorWindows();
}

