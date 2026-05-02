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
#include "Engine/Input/InputSystem.h"

#include "Editor/UI/ImGuiSetting.h"
#include "Editor/UI/NotificationToast.h"
#include "Editor/UI/EditorPanelTitleUtils.h"
#include "Core/ProjectSettings.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"

#include <filesystem>

namespace
{
	constexpr ImVec4 PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);

	void DrawPopupSectionHeader(const char* Label)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, PopupSectionHeaderTextColor);
		ImGui::SeparatorText(Label);
		ImGui::PopStyleColor();
	}

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

	void ApplyEditorColorTheme()
	{
		ImGuiStyle& Style = ImGui::GetStyle();
		Style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		Style.Colors[ImGuiCol_ChildBg] = ImVec4(0.03f, 0.03f, 0.03f, 1.0f);
		Style.Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.08f, 0.98f);
		Style.Colors[ImGuiCol_TitleBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		Style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		Style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		Style.Colors[ImGuiCol_FrameBg] = ImVec4(0.03f, 0.03f, 0.04f, 1.0f);
		Style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.07f, 0.07f, 0.09f, 1.0f);
		Style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
		Style.Colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.56f, 0.96f, 1.0f);
		Style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
		Style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.26f, 0.28f, 1.0f);
		Style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.32f, 0.35f, 1.0f);
		Style.Colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
		Style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.24f, 0.27f, 1.0f);
		Style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.28f, 0.31f, 1.0f);
	Style.Colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
	Style.Colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
	}

	FString GetSceneTitleLabel(UEditorEngine* EditorEngine)
	{
		if (!EditorEngine || !EditorEngine->HasCurrentLevelFilePath())
		{
			return "Untitled.Scene";
		}

		const std::filesystem::path ScenePath(FPaths::ToWide(EditorEngine->GetCurrentLevelFilePath()));
		const std::wstring FileName = ScenePath.filename().wstring();
		return FileName.empty() ? FString("Untitled.Scene") : FPaths::ToUtf8(FileName);
	}

	float GetCustomTitleBarHeight()
	{
		return 48.0f;
	}

	float GetWindowOuterPadding()
	{
		return 0.0f;
	}

	float GetWindowCornerRadius()
	{
		return 12.0f;
	}

	float GetWindowTopContentInset(FWindowsWindow* Window)
	{
		(void)Window;
		return 0.0f;
	}

	const char* GetWindowControlIconMinimize()
	{
		return "\xEE\xA4\xA1";
	}

	const char* GetWindowControlIconMaximize()
	{
		return "\xEE\xA4\xA2";
	}

	const char* GetWindowControlIconRestore()
	{
		return "\xEE\xA4\xA3";
	}

	const char* GetWindowControlIconClose()
	{
		return "\xEE\xA2\xBB";
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
	ApplyEditorColorTheme();
	ApplyEditorTabStyle();

	Window = InWindow;
	EditorEngine = InEditorEngine;

	ImGuiStyle& Style = ImGui::GetStyle();
	Style.WindowPadding.x = (std::max)(Style.WindowPadding.x, 12.0f);
	Style.WindowPadding.y = (std::max)(Style.WindowPadding.y, 10.0f);
	Style.FramePadding.x = (std::max)(Style.FramePadding.x, 8.0f);
	Style.FramePadding.y = (std::max)(Style.FramePadding.y, 5.0f);
	Style.ItemSpacing.x = (std::max)(Style.ItemSpacing.x, 10.0f);
	Style.ItemSpacing.y = (std::max)(Style.ItemSpacing.y, 8.0f);
	Style.CellPadding.x = (std::max)(Style.CellPadding.x, 8.0f);
	Style.CellPadding.y = (std::max)(Style.CellPadding.y, 6.0f);

	const FString FontPath = FResourceManager::Get().ResolvePath(FName("Default.Font.UI"));
	IO.Fonts->AddFontFromFileTTF(FontPath.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
	TitleBarFont = IO.Fonts->AddFontFromFileTTF(FontPath.c_str(), 18.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());
	EditorPanelTitleUtils::EnsurePanelChromeIconFontLoaded();
	if (std::filesystem::exists("C:/Windows/Fonts/segmdl2.ttf"))
	{
		ImFontConfig IconFontConfig{};
		IconFontConfig.PixelSnapH = true;
		WindowControlIconFont = IO.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segmdl2.ttf", 13.0f, &IconFontConfig);
	}

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
	EditorPanelTitleUtils::BeginPanelDecorationFrame();

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float TitleBarHeight = GetCustomTitleBarHeight();
	const float TopFrameInset = GetWindowTopContentInset(Window);
	const float OuterPadding = GetWindowOuterPadding();
	const float CornerRadius = GetWindowCornerRadius();
	const ImVec2 ViewportMin = MainViewport->Pos;
	const ImVec2 ViewportMax(MainViewport->Pos.x + MainViewport->Size.x, MainViewport->Pos.y + MainViewport->Size.y);
	const ImVec2 FrameMin(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + OuterPadding);
	const ImVec2 FrameMax(MainViewport->Pos.x + MainViewport->Size.x - OuterPadding, MainViewport->Pos.y + MainViewport->Size.y - OuterPadding);
	ImDrawList* BackgroundDrawList = ImGui::GetBackgroundDrawList(const_cast<ImGuiViewport*>(MainViewport));
	BackgroundDrawList->AddRectFilled(ViewportMin, ViewportMax, IM_COL32(0, 0, 0, 255));
	BackgroundDrawList->AddRectFilled(FrameMin, FrameMax, IM_COL32(0, 0, 0, 255), CornerRadius);

	RenderMainMenuBar();

	ImGuiWindowClass DockspaceWindowClass{};
	DockspaceWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
	ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + TitleBarHeight + OuterPadding), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(MainViewport->Size.x - OuterPadding * 2.0f, MainViewport->Size.y - TopFrameInset - TitleBarHeight - OuterPadding * 2.0f), ImGuiCond_Always);
	ImGui::SetNextWindowViewport(MainViewport->ID);
	ImGuiWindowFlags DockspaceWindowFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 1.0f));
	if (ImGui::Begin("##EditorDockSpaceHost", nullptr, DockspaceWindowFlags))
	{
		ImGui::DockSpace(ImGui::GetID("##EditorDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &DockspaceWindowClass);
	}
	ImGui::End();
	ImGui::PopStyleVar(4);

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

	RenderProjectSettingsWindow();

	RenderShortcutOverlay();
	EditorPanelTitleUtils::FlushPanelDecorations();

	// 토스트 알림 (항상 최상위에 표시)
	FNotificationToast::Render();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::RenderMainMenuBar()
{
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const float TitleBarHeight = GetCustomTitleBarHeight();
	const float TopFrameInset = GetWindowTopContentInset(Window);
	const float OuterPadding = GetWindowOuterPadding();
	const float CornerRadius = GetWindowCornerRadius();
	const float LogoSize = 36.0f;
	const float ButtonWidth = 38.0f;
	const float WindowControlHeight = 24.0f;
	const float ButtonSpacing = 2.0f;
	const float RightControlsWidth = ButtonWidth * 3.0f + ButtonSpacing * 2.0f;
	const float TitleBarPaddingY = 2.0f;
	const float LeftContentInset = 8.0f;

	ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + OuterPadding), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(MainViewport->Size.x - OuterPadding * 2.0f, TitleBarHeight), ImGuiCond_Always);
	ImGui::SetNextWindowViewport(MainViewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f + LeftContentInset, TitleBarPaddingY));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
	const ImGuiWindowFlags TitleBarFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;
	if (!ImGui::Begin("##EditorCustomTitleBar", nullptr, TitleBarFlags))
	{
		ImGui::End();
		ImGui::PopStyleVar(4);
		return;
	}

	FEditorSettings& Settings = FEditorSettings::Get();
	ID3D11ShaderResourceView* LogoTexture = FResourceManager::Get().FindLoadedTexture(
		FResourceManager::Get().ResolvePath(FName("Editor.Icon.AppLogo"))).Get();

	float MenuEndX = 54.0f;
	if (ImGui::BeginMenuBar())
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 7.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
		if (TitleBarFont)
		{
			ImGui::PushFont(TitleBarFont);
		}
		const FString SceneTabLabel = GetSceneTitleLabel(EditorEngine);
		FString ScenePathTooltip = "Unsaved Scene";
		if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
		{
			ScenePathTooltip = EditorEngine->GetCurrentLevelFilePath();
		}
		const float SceneTabWidth = ImGui::CalcTextSize(SceneTabLabel.c_str()).x + 34.0f;
		const float MenuFrameHeight = ImGui::GetFrameHeight();
		const float SceneTabHeight = MenuFrameHeight;
		const float MaxContentHeight = (std::max)((std::max)(MenuFrameHeight, SceneTabHeight), (std::max)(WindowControlHeight, LogoSize));
		const float ContentStartY = (std::max)(0.0f, floorf((TitleBarHeight - MaxContentHeight) * 0.5f) - 6.0f);
		const float RightControlsStartX = ImGui::GetWindowWidth() - RightControlsWidth;
		const float SceneTabX = RightControlsStartX - SceneTabWidth - 12.0f;
		float MenuStartX = ImGui::GetStyle().WindowPadding.x;

		if (LogoTexture)
		{
			const float LogoX = 8.0f;
			const float LogoY = ContentStartY;
			ImDrawList* DrawList = ImGui::GetForegroundDrawList(const_cast<ImGuiViewport*>(MainViewport));
			const ImVec2 WindowPos = ImGui::GetWindowPos();
			DrawList->AddImage(
				LogoTexture,
				ImVec2(WindowPos.x + LogoX, WindowPos.y + LogoY),
				ImVec2(WindowPos.x + LogoX + LogoSize, WindowPos.y + LogoY + LogoSize));
			MenuStartX = LogoX + LogoSize + 10.0f;
		}

		ImGui::SetCursorPos(ImVec2(MenuStartX, ContentStartY));

		if (ImGui::BeginMenu("File"))
		{
			DrawPopupSectionHeader("SCENE");
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
			DrawPopupSectionHeader("IMPORT");
			if (ImGui::MenuItem("Import Material...") && EditorEngine)
			{
				EditorEngine->ImportMaterialWithDialog();
			}
			if (ImGui::MenuItem("Import Texture...") && EditorEngine)
			{
				EditorEngine->ImportTextureWithDialog();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			const bool bCanUndo = EditorEngine && EditorEngine->CanUndoTransformChange();
			const bool bCanRedo = EditorEngine && EditorEngine->CanRedoTransformChange();
			if (!bCanUndo) ImGui::BeginDisabled();
			if (ImGui::MenuItem("Undo", "Ctrl+Z") && EditorEngine) EditorEngine->UndoTrackedTransformChange();
			if (!bCanUndo) ImGui::EndDisabled();
			if (!bCanRedo) ImGui::BeginDisabled();
			if (ImGui::MenuItem("Redo", "Ctrl+Y") && EditorEngine) EditorEngine->RedoTrackedTransformChange();
			if (!bCanRedo) ImGui::EndDisabled();
			ImGui::Separator();
			if (ImGui::MenuItem("Project Settings..."))
			{
				bShowProjectSettings = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
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
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("Shortcut Help"))
			{
				bShowShortcutOverlay = !bShowShortcutOverlay;
			}
			ImGui::EndMenu();
		}

		MenuEndX = ImGui::GetCursorPosX();

		ImGui::SetCursorPos(ImVec2(SceneTabX, ContentStartY));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.72f, 0.74f, 1.0f));
		ImGui::Button(SceneTabLabel.c_str(), ImVec2(SceneTabWidth, SceneTabHeight));
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(520.0f);
			ImGui::Text("Current: %s", ScenePathTooltip.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);

		if (Window)
		{
			ImGui::SetCursorPos(ImVec2(RightControlsStartX, ContentStartY));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.24f, 0.26f, 1.0f));
			if (WindowControlIconFont)
			{
				ImGui::PushFont(WindowControlIconFont);
			}
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.58f));
			if (ImGui::Button(GetWindowControlIconMinimize(), ImVec2(ButtonWidth, WindowControlHeight)))
			{
				Window->Minimize();
			}
			ImGui::PopStyleVar();
			ImGui::SameLine(0.0f, ButtonSpacing);
			if (ImGui::Button(Window->IsWindowMaximized() ? GetWindowControlIconRestore() : GetWindowControlIconMaximize(), ImVec2(ButtonWidth, WindowControlHeight)))
			{
				Window->ToggleMaximize();
			}
			ImGui::SameLine(0.0f, ButtonSpacing);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.16f, 0.16f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.58f, 0.10f, 0.10f, 1.0f));
			if (ImGui::Button(GetWindowControlIconClose(), ImVec2(ButtonWidth, WindowControlHeight)))
			{
				Window->Close();
			}
			if (WindowControlIconFont)
			{
				ImGui::PopFont();
			}
			ImGui::PopStyleColor(2);
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);
		}

		if (TitleBarFont)
		{
			ImGui::PopFont();
		}
		ImGui::PopStyleVar(4);
		ImGui::EndMenuBar();
	}

	const float SceneTabWidth = ImGui::CalcTextSize(GetSceneTitleLabel(EditorEngine).c_str()).x + 34.0f;
	const float DragRegionStartX = MenuEndX + 8.0f;
	const float DragRegionEndX = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - RightControlsWidth - SceneTabWidth - 20.0f;
	const float DragRegionWidth = DragRegionEndX - DragRegionStartX;

	if (Window && DragRegionWidth > 24.0f)
	{
		Window->SetTitleBarDragRegion(DragRegionStartX, 0.0f, DragRegionWidth, TitleBarHeight);
	}
	else if (Window)
	{
		Window->ClearTitleBarDragRegion();
	}

	ImGui::End();
	ImGui::PopStyleVar(4);
}

void FEditorMainPanel::RenderProjectSettingsWindow()
{
	if (!bShowProjectSettings)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(560.0f, 460.0f), ImGuiCond_Appearing);
	if (!ImGui::Begin("Project Settings", &bShowProjectSettings))
	{
		ImGui::End();
		return;
	}

	FProjectSettings& ProjectSettings = FProjectSettings::Get();

	DrawPopupSectionHeader("SHADOW");
	ImGui::Checkbox("Enable Shadows", &ProjectSettings.Shadow.bEnabled);
	ImGui::InputScalar("CSM Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.CSMResolution);
	ImGui::InputScalar("Spot Atlas Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.SpotAtlasResolution);
	ImGui::InputScalar("Point Atlas Resolution", ImGuiDataType_U32, &ProjectSettings.Shadow.PointAtlasResolution);
	ImGui::InputScalar("Max Spot Atlas Pages", ImGuiDataType_U32, &ProjectSettings.Shadow.MaxSpotAtlasPages);
	ImGui::InputScalar("Max Point Atlas Pages", ImGuiDataType_U32, &ProjectSettings.Shadow.MaxPointAtlasPages);

	DrawPopupSectionHeader("LIGHT CULLING");
	int32 LightCullingMode = static_cast<int32>(ProjectSettings.LightCulling.Mode);
	ImGui::RadioButton("Off", &LightCullingMode, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Tile", &LightCullingMode, 1);
	ImGui::SameLine();
	ImGui::RadioButton("Cluster", &LightCullingMode, 2);
	ProjectSettings.LightCulling.Mode = static_cast<uint32>(LightCullingMode);
	ImGui::SliderFloat("Heat Map Max", &ProjectSettings.LightCulling.HeatMapMax, 1.0f, 100.0f, "%.0f");
	ImGui::Checkbox("Enable 2.5D Culling", &ProjectSettings.LightCulling.bEnable25DCulling);

	DrawPopupSectionHeader("SCENE DEPTH");
	int32 SceneDepthMode = static_cast<int32>(ProjectSettings.SceneDepth.Mode);
	ImGui::Combo("Mode", &SceneDepthMode, "Power\0Linear\0");
	ProjectSettings.SceneDepth.Mode = static_cast<uint32>(SceneDepthMode);
	ImGui::SliderFloat("Exponent", &ProjectSettings.SceneDepth.Exponent, 1.0f, 512.0f, "%.0f");

	if (ImGui::Button("Save"))
	{
		ProjectSettings.SaveToFile(FProjectSettings::GetDefaultPath());
	}
	ImGui::SameLine();
	if (ImGui::Button("Close"))
	{
		bShowProjectSettings = false;
	}

	ImGui::End();
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
	ImGui::TextUnformatted("Ctrl+Z : Undo Scene Change");
	ImGui::TextUnformatted("Ctrl+Y : Redo Scene Change");
	ImGui::TextUnformatted("` : Toggle Console");
	ImGui::TextUnformatted("Ctrl+Space : Toggle Content Browser");
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
	if (EditorEngine && EditorEngine->IsMouseOverViewport())
	{
		bWantMouse = false;
		if (!IO.WantTextInput && !bShowShortcutOverlay)
		{
			bWantKeyboard = false;
		}
	}
	InputSystem::Get().GetGuiInputState().bUsingMouse = bWantMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard = bWantKeyboard;
	InputSystem::Get().GetGuiInputState().bUsingTextInput = IO.WantTextInput;

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

	InputSystem& Input = InputSystem::Get();
	FEditorSettings& Settings = FEditorSettings::Get();

	if (Input.GetKeyDown(VK_OEM_3))
	{
		Settings.UI.bConsole = !Settings.UI.bConsole;
		return;
	}

	if (!Input.GetKey(VK_CONTROL))
	{
		return;
	}

	const bool bShift = Input.GetKey(VK_SHIFT);
	if (Input.GetKeyDown(VK_SPACE))
	{
		Settings.UI.bContentBrowser = !Settings.UI.bContentBrowser;
		return;
	}

	if (Input.GetKeyDown('N'))
	{
		EditorEngine->NewScene();
	}
	else if (Input.GetKeyDown('O'))
	{
		EditorEngine->LoadSceneWithDialog();
	}
	else if (Input.GetKeyDown('S'))
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
	else if (Input.GetKeyDown('Z'))
	{
		EditorEngine->UndoTrackedTransformChange();
	}
	else if (Input.GetKeyDown('Y'))
	{
		EditorEngine->RedoTrackedTransformChange();
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
