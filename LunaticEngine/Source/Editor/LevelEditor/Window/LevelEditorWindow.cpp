#include "MainFrame/EditorMainFrame.h"

#include "Component/CameraComponent.h"
#include "EditorEngine.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "LevelEditor/Subsystem/OverlayStatSystem.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "Object/Object.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"

#include "Engine/Input/InputManager.h"
#include "Render/Pipeline/Renderer.h"

#include "Common/File/EditorFileUtils.h"
#include "Common/UI/EditorAccentColor.h"
#include "Common/UI/EditorPanelTitleUtils.h"
#include "Common/UI/NotificationToast.h"
#include "Core/Notification.h"
#include "Core/ProjectSettings.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Level.h"
#include "MainFrame/ImGuiSetting.h"
#include "Object/UClass.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"

#include <filesystem>
#include <windows.h>

namespace
{
    constexpr ImVec4           UnrealPanelSurface = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
    constexpr ImVec4           UnrealPanelSurfaceHover = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
    constexpr ImVec4           UnrealPanelSurfaceActive = ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f);
    constexpr ImVec4           UnrealDockEmpty = ImVec4(5.0f / 255.0f, 5.0f / 255.0f, 5.0f / 255.0f, 1.0f);
    constexpr ImVec4           UnrealPopupSurface = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
    constexpr ImVec4           UnrealBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
    constexpr ImVec4           PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
    constexpr EOverlayStatType SupportedOverlayStats[] = {
        EOverlayStatType::FPS,
        EOverlayStatType::PickingTime,
        EOverlayStatType::Memory,
        EOverlayStatType::Shadow,
    };
    constexpr const char *CreditsDevelopers[] = {
        "Hojin Lee",
        "HyoBeom Kim",
        "Hyungjun Kim",
        "JunHyeop3631",
        "keonwookang0914",
        "kimhojun",
        "kwonhyeonsoo-goo",
        "LEE SangHoon",
        "lin-ion",
        "Park SangHyeok",
        "Seyoung Park",
        "ShimWoojin",
        "wwonnn",
        "Yonaim",
        "\xEA\xB0\x95\xEA\xB1\xB4\xEC\x9A\xB0",
        "\xEA\xB9\x80\xED\x83\x9C\xED\x98\x84",
        "\xEB\x82\xA8\xEC\x9C\xA4\xEC\xA7\x80",
        "\xEC\xA1\xB0\xED\x98\x84\xEC\x84\x9D",
    };

    void DrawPopupSectionHeader(const char *Label)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, PopupSectionHeaderTextColor);
        ImGui::TextUnformatted(Label);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    void SetNextPopupWindowPosition(ImGuiCond Condition = ImGuiCond_Appearing)
    {
        if (const ImGuiViewport *MainViewport = ImGui::GetMainViewport())
        {
            const ImVec2 PopupAnchor(MainViewport->Pos.x + MainViewport->Size.x * 0.5f, MainViewport->Pos.y + MainViewport->Size.y * 0.42f);
            ImGui::SetNextWindowPos(PopupAnchor, Condition, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowViewport(MainViewport->ID);
        }
    }

    bool BeginUtilityPopupWindow(const char *Title, bool *bOpen, const ImVec2 &InitialSize, ImGuiCond SizeCondition,
                                 ImGuiWindowFlags Flags = 0)
    {
        SetNextPopupWindowPosition(ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(InitialSize, SizeCondition);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, UnrealPanelSurfaceHover);
        ImGui::PushStyleColor(ImGuiCol_Border, UnrealBorder);
        const bool bVisible = ImGui::Begin(Title, bOpen, Flags);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        return bVisible;
    }

    bool ConfirmNewScene(HWND OwnerWindowHandle)
    {
        const int32 Result = MessageBoxW(OwnerWindowHandle, L"Create a new scene?\nUnsaved changes may be lost.", L"New Scene",
                                         MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        return Result == IDYES;
    }

    void ApplyEditorTabStyle()
    {
        ImGuiStyle &Style = ImGui::GetStyle();
        Style.TabRounding = (std::max)(Style.TabRounding, 9.0f);
        Style.TabBorderSize = (std::max)(Style.TabBorderSize, 1.0f);
        Style.TabBarBorderSize = 0.0f;
        Style.TabBarOverlineSize = 0.0f;
        Style.DockingSeparatorSize = 2.0f;

        Style.Colors[ImGuiCol_Tab] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabHovered] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabSelected] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabDimmed] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabDimmedSelected] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TabSelectedOverline] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_TabDimmedSelectedOverline] = UnrealDockEmpty;
    }

    void ApplyEditorColorTheme()
    {
        ImGuiStyle &Style = ImGui::GetStyle();
        Style.Colors[ImGuiCol_WindowBg] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_ChildBg] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_PopupBg] = UnrealPopupSurface;
        Style.Colors[ImGuiCol_TitleBg] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TitleBgActive] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_TitleBgCollapsed] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_MenuBarBg] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_FrameBg] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_FrameBgHovered] = UnrealPanelSurfaceHover;
        Style.Colors[ImGuiCol_FrameBgActive] = UnrealPanelSurfaceActive;
        Style.Colors[ImGuiCol_CheckMark] = EditorAccentColor::Value;
        Style.Colors[ImGuiCol_Button] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_ButtonHovered] = UnrealPanelSurfaceHover;
        Style.Colors[ImGuiCol_ButtonActive] = UnrealPanelSurfaceActive;
        Style.Colors[ImGuiCol_Header] = UnrealPanelSurface;
        Style.Colors[ImGuiCol_HeaderHovered] = UnrealPanelSurfaceHover;
        Style.Colors[ImGuiCol_HeaderActive] = UnrealPanelSurfaceActive;
        Style.Colors[ImGuiCol_Separator] = UnrealDockEmpty;
        Style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(12.0f / 255.0f, 12.0f / 255.0f, 12.0f / 255.0f, 1.0f);
        Style.Colors[ImGuiCol_SeparatorActive] = ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);
        Style.Colors[ImGuiCol_Border] = UnrealBorder;
        Style.Colors[ImGuiCol_DockingEmptyBg] = UnrealDockEmpty;
    }

    FString GetSceneTitleLabel(UEditorEngine *EditorEngine)
    {
        if (!EditorEngine || !EditorEngine->HasCurrentLevelFilePath())
        {
            return "Untitled.Scene";
        }

        const std::filesystem::path ScenePath(FPaths::ToWide(EditorEngine->GetCurrentLevelFilePath()));
        const std::wstring          FileName = ScenePath.filename().wstring();
        return FileName.empty() ? FString("Untitled.Scene") : FPaths::ToUtf8(FileName);
    }

    float GetCustomTitleBarHeight() { return 42.0f; }

    float GetWindowOuterPadding() { return 6.0f; }

    float GetWindowCornerRadius() { return 12.0f; }

    float GetWindowTopContentInset(FWindowsWindow *Window)
    {
        (void)Window;
        return 0.0f;
    }

    const char *GetWindowControlIconMinimize() { return "\xEE\xA4\xA1"; }

    const char *GetWindowControlIconMaximize() { return "\xEE\xA4\xA2"; }

    const char *GetWindowControlIconRestore() { return "\xEE\xA4\xA3"; }

    const char *GetWindowControlIconClose() { return "\xEE\xA2\xBB"; }

} // namespace

void FLevelEditorWindow::Create(FWindowsWindow *InWindow, FRenderer &InRenderer, UEditorEngine *InEditorEngine, FLevelEditor *InLevelEditor,
                                FEditorMainFrame *InMainFrame)
{
    Window = InWindow;
    EditorEngine = InEditorEngine;
    LevelEditor = InLevelEditor;
    MainFrame = InMainFrame;

    ConsolePanel.Init(InEditorEngine);
    DetailsPanel.Init(InEditorEngine);
    OutlinerPanel.Init(InEditorEngine);
    PlaceActorsPanel.Init(InEditorEngine);
    StatPanel.Init(InEditorEngine);
    ContentBrowser.Init(InEditorEngine, InRenderer.GetFD3DDevice().GetDevice());
    ShadowMapDebugPanel.Init(InEditorEngine);
}

void FLevelEditorWindow::Release()
{
    ConsolePanel.Shutdown();
}

void FLevelEditorWindow::SaveToSettings() const { ContentBrowser.SaveToSettings(); }

void FLevelEditorWindow::FlushPendingMenuAction()
{
    const EPendingMenuAction Action = PendingMenuAction;
    PendingMenuAction = EPendingMenuAction::None;

    switch (Action)
    {
    case EPendingMenuAction::None:
        return;
    case EPendingMenuAction::NewScene:
        if (EditorEngine)
        {
            HWND OwnerWindowHandle = Window ? Window->GetHWND() : nullptr;
            if (ConfirmNewScene(OwnerWindowHandle))
            {
                EditorEngine->NewScene();
            }
        }
        return;
    case EPendingMenuAction::OpenScene:
        if (EditorEngine)
        {
            EditorEngine->LoadSceneWithDialog();
        }
        return;
    case EPendingMenuAction::SaveScene:
        if (EditorEngine)
        {
            EditorEngine->SaveScene();
        }
        return;
    case EPendingMenuAction::SaveSceneAs:
        if (EditorEngine)
        {
            EditorEngine->RequestSaveSceneAsDialog();
        }
        return;
    case EPendingMenuAction::NewUAsset:
        if (EditorEngine)
        {
            EditorEngine->GetAssetEditorManager().CreateCameraModifierStackAsset();
        }
        return;
    case EPendingMenuAction::OpenUAsset:
        if (EditorEngine)
        {
            EditorEngine->GetAssetEditorManager().OpenAssetWithDialog(Window ? Window->GetHWND() : nullptr);
        }
        return;
    case EPendingMenuAction::ImportMaterial:
        if (EditorEngine)
        {
            EditorEngine->ImportMaterialWithDialog();
        }
        return;
    case EPendingMenuAction::ImportTexture:
        if (EditorEngine)
        {
            EditorEngine->ImportTextureWithDialog();
        }
        return;
    case EPendingMenuAction::CookCurrentScene:
        CookCurrentScene();
        return;
    case EPendingMenuAction::CookAllScenes:
    {
        const int32 Count = FSceneSaveManager::CookAllScenes();
        FNotificationManager::Get().AddNotification(std::string("Cooked ") + std::to_string(Count) + " scenes",
                                                    Count > 0 ? ENotificationType::Success : ENotificationType::Error);
        return;
    }
    case EPendingMenuAction::PackageRelease:
        PackageGameBuild("ReleaseBuild.bat");
        return;
    case EPendingMenuAction::PackageShipping:
        PackageGameBuild("ShippingBuild.bat");
        return;
    case EPendingMenuAction::PackageDemo:
        PackageGameBuild("DemoBuild.bat");
        return;
    }
}

void FLevelEditorWindow::Render(float DeltaTime)
{
    EditorPanelTitleUtils::BeginPanelDecorationFrame();

    const ImGuiViewport *MainViewport = ImGui::GetMainViewport();
    const float          TitleBarHeight = GetCustomTitleBarHeight();
    const float          TopFrameInset = GetWindowTopContentInset(Window);
    const float          OuterPadding = GetWindowOuterPadding();
    const float          CornerRadius = GetWindowCornerRadius();
    const ImVec2         ViewportMin = MainViewport->Pos;
    const ImVec2         ViewportMax(MainViewport->Pos.x + MainViewport->Size.x, MainViewport->Pos.y + MainViewport->Size.y);
    const ImVec2         FrameMin(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + OuterPadding);
    const ImVec2         FrameMax(MainViewport->Pos.x + MainViewport->Size.x - OuterPadding,
                                  MainViewport->Pos.y + MainViewport->Size.y - OuterPadding);
    ImDrawList          *BackgroundDrawList = ImGui::GetBackgroundDrawList(const_cast<ImGuiViewport *>(MainViewport));
    BackgroundDrawList->AddRectFilled(ViewportMin, ViewportMax, IM_COL32(5, 5, 5, 255));
    BackgroundDrawList->AddRectFilled(FrameMin, FrameMax, IM_COL32(5, 5, 5, 255), CornerRadius);

    ImGuiWindowClass DockspaceWindowClass{};
    DockspaceWindowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
    ImGui::SetNextWindowPos(ImVec2(MainViewport->Pos.x + OuterPadding, MainViewport->Pos.y + TopFrameInset + TitleBarHeight + OuterPadding),
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(MainViewport->Size.x - OuterPadding * 2.0f, MainViewport->Size.y - TopFrameInset - TitleBarHeight - OuterPadding * 2.0f),
        ImGuiCond_Always);
    ImGui::SetNextWindowViewport(MainViewport->ID);
    ImGuiWindowFlags DockspaceWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 6.0f));
    if (ImGui::Begin("##EditorDockSpaceHost", nullptr, DockspaceWindowFlags))
    {
        ImGui::DockSpace(ImGui::GetID("##EditorDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &DockspaceWindowClass);
    }
    ImGui::End();
    ImGui::PopStyleVar(4);

    // 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
    const FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    if (EditorEngine && Settings.Panels.bViewport)
    {
        SCOPE_STAT_CAT("EditorEngine->RenderViewportUI", "5_UI");
        EditorEngine->RenderViewportUI(DeltaTime);

        if (FLevelEditorViewportClient *ActiveViewport = EditorEngine->GetActiveViewport())
        {
            EditorEngine->GetOverlayStatSystem().RenderImGui(*EditorEngine, ActiveViewport->GetViewportScreenRect());
        }
    }

    if (!bHideEditorWindows && Settings.Panels.bImGuiSettings)
    {
        ImGuiSetting::ShowSetting();
    }

    if (!bHideEditorWindows && Settings.Panels.bConsole)
    {
        SCOPE_STAT_CAT("ConsolePanel.Render", "5_UI");
        ConsolePanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.Panels.bDetails)
    {
        SCOPE_STAT_CAT("DetailsPanel.Render", "5_UI");
        DetailsPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.Panels.bOutliner)
    {
        SCOPE_STAT_CAT("OutlinerPanel.Render", "5_UI");
        OutlinerPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.Panels.bPlaceActors)
    {
        SCOPE_STAT_CAT("PlaceActorsPanel.Render", "5_UI");
        PlaceActorsPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.Panels.bStats)
    {
        SCOPE_STAT_CAT("StatPanel.Render", "5_UI");
        StatPanel.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.Panels.bContentBrowser)
    {
        SCOPE_STAT_CAT("ContentBrowser.Render", "5_UI");
        ContentBrowser.Render(DeltaTime);
    }

    if (!bHideEditorWindows && Settings.Panels.bShadowMapDebug)
    {
        ShadowMapDebugPanel.Render(DeltaTime);
    }

    if (EditorEngine)
    {
        EditorEngine->RenderPIEOverlayPopups();
    }

    EditorPanelTitleUtils::FlushPanelDecorations();

    // 토스트 알림 (항상 최상위에 표시)
    FNotificationToast::Render();

}

void FLevelEditorWindow::BuildFileMenu()
{
    DrawPopupSectionHeader("SCENE");
    if (ImGui::MenuItem("New Scene", "Ctrl+N") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::NewScene;
        return;
    }
    if (ImGui::MenuItem("Open Scene...", "Ctrl+O") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::OpenScene;
        return;
    }
    if (ImGui::MenuItem("Save Scene", "Ctrl+S") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::SaveScene;
        return;
    }
    if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::SaveSceneAs;
        return;
    }

    DrawPopupSectionHeader("ASSET");
    if (ImGui::MenuItem("New UAsset...") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::NewUAsset;
        return;
    }
    if (ImGui::MenuItem("Open UAsset...", "Ctrl+Alt+O") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::OpenUAsset;
        return;
    }

    DrawPopupSectionHeader("IMPORT");
    if (ImGui::MenuItem("Import Material...") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::ImportMaterial;
        return;
    }
    if (ImGui::MenuItem("Import Texture...") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::ImportTexture;
        return;
    }

    DrawPopupSectionHeader("COOK");
    if (ImGui::MenuItem("Cook Current Scene") && EditorEngine)
    {
        PendingMenuAction = EPendingMenuAction::CookCurrentScene;
        return;
    }
    if (ImGui::MenuItem("Cook All Scenes"))
    {
        PendingMenuAction = EPendingMenuAction::CookAllScenes;
        return;
    }

    DrawPopupSectionHeader("PACKAGE");
    if (ImGui::MenuItem("Package: Release..."))
    {
        PendingMenuAction = EPendingMenuAction::PackageRelease;
        return;
    }
    if (ImGui::MenuItem("Package: Shipping..."))
    {
        PendingMenuAction = EPendingMenuAction::PackageShipping;
        return;
    }
    if (ImGui::MenuItem("Package: Demo..."))
    {
        PendingMenuAction = EPendingMenuAction::PackageDemo;
        return;
    }
}

void FLevelEditorWindow::BuildEditMenu()
{
    const bool bCanUndo = EditorEngine && EditorEngine->CanUndoTransformChange();
    const bool bCanRedo = EditorEngine && EditorEngine->CanRedoTransformChange();
    if (!bCanUndo)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("Undo", "Ctrl+Z") && EditorEngine)
        EditorEngine->UndoTrackedTransformChange();
    if (!bCanUndo)
        ImGui::EndDisabled();
    if (!bCanRedo)
        ImGui::BeginDisabled();
    if (ImGui::MenuItem("Redo", "Ctrl+Y") && EditorEngine)
        EditorEngine->RedoTrackedTransformChange();
    if (!bCanRedo)
        ImGui::EndDisabled();
}

void FLevelEditorWindow::BuildWindowMenu()
{
    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    ImGui::Checkbox("Viewport", &Settings.Panels.bViewport);
    ImGui::Separator();
    ImGui::Checkbox("Console", &Settings.Panels.bConsole);
    ImGui::Checkbox("Details", &Settings.Panels.bDetails);
    ImGui::Checkbox("Outliner", &Settings.Panels.bOutliner);
    ImGui::Checkbox("Place Actors", &Settings.Panels.bPlaceActors);
    ImGui::Checkbox("Stat Profiler", &Settings.Panels.bStats);
    ImGui::Checkbox("Content Browser", &Settings.Panels.bContentBrowser);
    ImGui::Checkbox("Shadow Map Debug", &Settings.Panels.bShadowMapDebug);
    ImGui::Separator();
    ImGui::Checkbox("ImGuiSetting", &Settings.Panels.bImGuiSettings);
}

void FLevelEditorWindow::BuildCustomMenus()
{
    if (ImGui::BeginMenu("Stat"))
    {
        if (EditorEngine)
        {
            FOverlayStatSystem &OverlayStats = EditorEngine->GetOverlayStatSystem();
            for (EOverlayStatType StatType : SupportedOverlayStats)
            {
                bool bVisible = OverlayStats.IsStatVisible(StatType);
                if (ImGui::MenuItem(FOverlayStatSystem::GetStatDisplayName(StatType), nullptr, bVisible))
                {
                    OverlayStats.SetStatVisible(StatType, !bVisible);
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Hide All"))
            {
                OverlayStats.HideAll();
            }
        }
        else
        {
            ImGui::BeginDisabled();
            for (EOverlayStatType StatType : SupportedOverlayStats)
            {
                ImGui::MenuItem(FOverlayStatSystem::GetStatDisplayName(StatType), nullptr, false, false);
            }
            ImGui::Separator();
            ImGui::MenuItem("Hide All", nullptr, false, false);
            ImGui::EndDisabled();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Levels"))
    {
        UWorld *World = EditorEngine ? EditorEngine->GetWorld() : nullptr;
        if (World)
        {
            ULevel *Persistent = World->GetPersistentLevel();
            FString PersistentName = Persistent ? "Persistent Level" : "No Persistent Level";
            bool bIsPersistentCurrent = (World->GetCurrentLevel() == Persistent);
            if (ImGui::MenuItem(PersistentName.c_str(), nullptr, bIsPersistentCurrent))
            {
                World->SetCurrentLevel(Persistent);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Streaming Levels");

            for (const auto &Info : World->GetStreamingLevels())
            {
                bool bIsCurrent = (World->GetCurrentLevel() == Info.LoadedLevel);
                FString DisplayName = Info.LevelName.ToString() + (Info.bIsLoaded ? "" : " (Unloaded)");

                if (ImGui::MenuItem(DisplayName.c_str(), nullptr, bIsCurrent))
                {
                    if (Info.LoadedLevel)
                    {
                        World->SetCurrentLevel(Info.LoadedLevel);
                    }
                }

                if (ImGui::BeginPopupContextItem())
                {
                    if (!Info.bIsLoaded)
                    {
                        if (ImGui::MenuItem("Load Level"))
                        {
                            World->LoadStreamingLevel(Info.LevelPath);
                        }
                    }
                    else if (ImGui::MenuItem("Unload Level"))
                    {
                        World->UnloadStreamingLevel(Info.LevelName);
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
                    .OwnerWindowHandle = Window ? Window->GetHWND() : nullptr,
                    .bFileMustExist = true,
                    .bPathMustExist = true,
                    .bPromptOverwrite = false,
                    .bReturnRelativeToProjectRoot = false,
                });
                if (!SelectedPath.empty())
                {
                    World->AddStreamingLevel(SelectedPath);
                }
            }

            if (Persistent && ImGui::BeginMenu("GameMode Override"))
            {
                const TArray<UClass *> Candidates = UClass::GetSubclassesOf(AGameModeBase::StaticClass());
                const FString CurrentName = Persistent->GetGameModeClassName();

                if (ImGui::MenuItem("(Use Project Default)", nullptr, CurrentName.empty()))
                {
                    Persistent->SetGameModeClassName("");
                }
                ImGui::Separator();
                for (UClass *C : Candidates)
                {
                    const bool bSelected = (CurrentName == C->GetName());
                    if (ImGui::MenuItem(C->GetName(), nullptr, bSelected))
                    {
                        Persistent->SetGameModeClassName(C->GetName());
                    }
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMenu();
    }
}

FString FLevelEditorWindow::GetFrameTitle() const
{
    return GetSceneTitleLabel(EditorEngine);
}

FString FLevelEditorWindow::GetFrameTitleTooltip() const
{
    if (EditorEngine && EditorEngine->HasCurrentLevelFilePath())
    {
        return EditorEngine->GetCurrentLevelFilePath();
    }

    return "Unsaved Scene";
}

void FLevelEditorWindow::Update()
{
    HandleGlobalShortcuts();
}

void FLevelEditorWindow::HandleGlobalShortcuts()
{
    if (!EditorEngine)
    {
        return;
    }
    if (EditorEngine->IsPIEPossessedMode())
    {
        return;
    }

    ImGuiIO &IO = ImGui::GetIO();
    if (IO.WantTextInput)
    {
        return;
    }

    FInputManager   &Input = FInputManager::Get();
    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();

    if (Input.IsKeyPressed(VK_OEM_3))
    {
        Settings.Panels.bConsole = !Settings.Panels.bConsole;
        return;
    }

    if (!Input.IsKeyDown(VK_CONTROL))
    {
        return;
    }

    const bool bShift = Input.IsKeyDown(VK_SHIFT);
    if (Input.IsKeyPressed(VK_SPACE))
    {
        Settings.Panels.bContentBrowser = !Settings.Panels.bContentBrowser;
        return;
    }

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
            EditorEngine->RequestSaveSceneAsDialog();
        }
        else
        {
            EditorEngine->SaveScene();
        }
    }
    else if (Input.IsKeyPressed('Z'))
    {
        EditorEngine->UndoTrackedTransformChange();
    }
    else if (Input.IsKeyPressed('Y'))
    {
        EditorEngine->RedoTrackedTransformChange();
    }
}

void FLevelEditorWindow::HideEditorWindows()
{
    if (bHasSavedPanelVisibility)
    {
        bHideEditorWindows = true;
        bShowPanelList = false;
        return;
    }

    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    SavedPanelVisibility = Settings.Panels;
    bSavedShowPanelList = bShowPanelList;
    bHasSavedPanelVisibility = true;
    bHideEditorWindows = true;
    bShowPanelList = false;

    Settings.Panels.bConsole = false;
    Settings.Panels.bDetails = false;
    Settings.Panels.bOutliner = false;
    Settings.Panels.bPlaceActors = false;
    Settings.Panels.bStats = false;
    Settings.Panels.bContentBrowser = false;
    Settings.Panels.bImGuiSettings = false;
    Settings.Panels.bShadowMapDebug = false;
}

void FLevelEditorWindow::ShowEditorWindows()
{
    if (!bHasSavedPanelVisibility)
    {
        bHideEditorWindows = false;
        return;
    }

    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
    Settings.Panels = SavedPanelVisibility;
    bShowPanelList = bSavedShowPanelList;
    bHideEditorWindows = false;
    bHasSavedPanelVisibility = false;
}

void FLevelEditorWindow::HideEditorWindowsForPIE() { HideEditorWindows(); }

void FLevelEditorWindow::RestoreEditorWindowsAfterPIE() { ShowEditorWindows(); }

void FLevelEditorWindow::CookCurrentScene()
{
    if (!EditorEngine || !EditorEngine->HasCurrentLevelFilePath())
    {
        FNotificationManager::Get().AddNotification("Cook: save the current scene first.", ENotificationType::Error);
        return;
    }

    const FString        &InPath = EditorEngine->GetCurrentLevelFilePath();
    std::filesystem::path Out(FPaths::ToWide(InPath));
    Out.replace_extension(L".umap");
    const FString OutPath = FPaths::ToUtf8(Out.wstring());

    const bool bOk = FSceneSaveManager::CookSceneToBinary(InPath, OutPath);
    FNotificationManager::Get().AddNotification(bOk ? std::string("Cooked: ") + Out.filename().string()
                                                    : std::string("Cook failed: ") + Out.filename().string(),
                                                bOk ? ENotificationType::Success : ENotificationType::Error);
}

void FLevelEditorWindow::PackageGameBuild(const char *BatFileName)
{
    // 솔루션 루트(.bat 위치)를 찾는다 — 후보 경로를 차례대로 검사.
    // FPaths::RootDir()은 보통 LunaticEngine/ (개발) 또는 exe 디렉터리(배포)를 반환한다.
    // 트레일링 슬래시 때문에 parent_path()가 의도대로 안 나올 수 있으므로 lexically_normal로 정규화.
    std::filesystem::path RootDir = std::filesystem::path(FPaths::RootDir()).lexically_normal();

    std::filesystem::path       SolutionDir;
    std::filesystem::path       BatPath;
    const std::filesystem::path Candidates[] = {
        RootDir,                             // exe 디렉터리에 .bat이 있는 경우 (배포)
        RootDir.parent_path(),               // LunaticEngine/의 상위 = 솔루션 루트 (개발)
        RootDir.parent_path().parent_path(), // 한 단계 더 (혹시 모를 중첩)
        std::filesystem::current_path(),     // 마지막 폴백
    };
    for (const auto &Candidate : Candidates)
    {
        const std::filesystem::path Tentative = Candidate / BatFileName;
        if (std::filesystem::exists(Tentative))
        {
            SolutionDir = Candidate;
            BatPath = Tentative;
            break;
        }
    }

    if (BatPath.empty())
    {
        FNotificationManager::Get().AddNotification(std::string("Package script not found: ") + BatFileName + " (searched near " +
                                                        RootDir.string() + ")",
                                                    ENotificationType::Error);
        return;
    }

    // .bat은 별도 콘솔 창에서 실행 (편집 중인 에디터를 막지 않게).
    // "cmd /c start \"Title\" /D <SolutionDir> cmd /k \"<bat>\"" 형태로 cmd 콘솔에서 띄움.
    std::wstring SolutionDirW = SolutionDir.wstring();
    std::wstring BatPathW = BatPath.wstring();

    std::wstring CommandLine = L"/c start \"Package Game Build\" /D \"" + SolutionDirW + L"\" cmd /k \"\"" + BatPathW + L"\"\"";

    HINSTANCE Result =
        ShellExecuteW(Window ? Window->GetHWND() : nullptr, L"open", L"cmd.exe", CommandLine.c_str(), SolutionDirW.c_str(), SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(Result) <= 32)
    {
        FNotificationManager::Get().AddNotification(std::string("Failed to launch package script: ") + BatFileName,
                                                    ENotificationType::Error);
        return;
    }

    FNotificationManager::Get().AddNotification(std::string("Packaging started: ") + BatFileName, ENotificationType::Info);
}
