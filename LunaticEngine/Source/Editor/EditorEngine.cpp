#include "EditorEngine.h"

#include "Audio/AudioManager.h"
#include "Common/File/EditorFileUtils.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "Core/Notification.h"
#include "Core/ProjectSettings.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/AActor.h"
#include "LevelEditor/History/SceneHistoryBuilder.h"
#include "LevelEditor/PIE/LevelPIEManager.h"
#include "LevelEditor/Render/EditorRenderPipeline.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "Materials/MaterialManager.h"
#include "Mesh/ObjManager.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Profiling/StartupProfiler.h"
#include "Texture/Texture2D.h"
#include <filesystem>
#include <fstream>
#include <set>

IMPLEMENT_CLASS(UEditorEngine, UEngine)

namespace
{
} // namespace

void UEditorEngine::Init(FWindowsWindow *InWindow)
{

    GIsEditor = true;

    // 엔진 공통 초기화 (Renderer, D3D, 싱글턴 등)
    UEngine::Init(InWindow);

    if (InWindow)
    {
        FInputManager::Get().SetOwnerWindow(InWindow->GetHWND());
    }

    {
        SCOPE_STARTUP_STAT("ObjManager::ScanMeshAssets");
        FObjManager::ScanMeshAssets();
        FObjManager::ScanObjSourceFiles();
    }

    {
        SCOPE_STARTUP_STAT("MaterialManager::ScanAssets");
        FMaterialManager::Get().ScanMaterialAssets();
    }
    UTexture2D::ScanTextureAssets();

    // 에디터 전용 초기화
    FEditorSettings::Get().LoadFromFile(FEditorSettings::GetDefaultSettingsPath());
    FProjectSettings::Get().LoadFromFile(FProjectSettings::GetDefaultPath());

    {
        SCOPE_STARTUP_STAT("EditorMainFrame::Create");
        MainFrame.Create(Window, Renderer, this);
    }

    SceneManager.Init(this);
    HistoryManager.Init(this);
    AssetImportManager.Init(this);

    // 기본 월드 생성 — 모든 서브시스템 초기화의 기반
    CreateWorldContext(EWorldType::Editor, FName("Default"));
    SetActiveWorld(WorldList[0].ContextHandle);
    GetWorld()->InitWorld();

    // Selection & Gizmo
    LevelEditor.Init(this, Window, Renderer);

    {
        SCOPE_STARTUP_STAT("Editor::LoadStartLevel");
        SceneManager.LoadStartLevel();
    }
    ApplyTransformSettingsToGizmo();

    // Editor render pipeline
    {
        SCOPE_STARTUP_STAT("EditorRenderPipeline::Create");
        SetRenderPipeline(std::make_unique<FEditorRenderPipeline>(this, Renderer));
    }
}

void UEditorEngine::Shutdown()
{
    // 에디터 해제 (엔진보다 먼저)
    LevelEditor.GetViewportLayout().SaveToSettings();
    MainFrame.SaveToSettings();
    FProjectSettings::Get().SaveToFile(FProjectSettings::GetDefaultPath());
    FEditorSettings::Get().SaveToFile(FEditorSettings::GetDefaultSettingsPath());
    CloseScene();
    AssetImportManager.Shutdown();
    HistoryManager.Shutdown();
    SceneManager.Shutdown();
    LevelEditor.Shutdown();
    MainFrame.Release();

    // 엔진 공통 해제 (Renderer, D3D 등)
    UEngine::Shutdown();
}

void UEditorEngine::OnWindowResized(uint32 Width, uint32 Height)
{
    UEngine::OnWindowResized(Width, Height);
    // 윈도우 리사이즈 시에는 ImGui 패널이 실제 크기를 결정하므로
    // FViewport RT는 SSplitter 레이아웃에서 지연 리사이즈로 처리됨
}

void UEditorEngine::Tick(float DeltaTime)
{
    if (!PendingSceneLoadReference.empty())
    {
        const FString SceneToLoad = PendingSceneLoadReference;
        PendingSceneLoadReference.clear();
        LoadScene(SceneToLoad);
    }
    LevelEditor.GetPIEManager().Tick(DeltaTime);
    SceneManager.Tick(DeltaTime);

    ApplyTransformSettingsToGizmo();
    FDirectoryWatcher::Get().ProcessChanges();
    if (UTexture2D::HasPendingTextureRefresh())
    {
        UTexture2D::RefreshChangedTextures(Renderer.GetFD3DDevice().GetDevice());
    }
    FNotificationManager::Get().Tick(DeltaTime);
    FAudioManager::Get().Update();
    MainFrame.Update();

    for (FEditorViewportClient *VC : GetViewportLayout().GetAllViewportClients())
    {
        VC->Tick(DeltaTime);
    }

    WorldTick(DeltaTime);
    Render(DeltaTime);

    if (!IsPIEPossessedMode())
    {
        GetSelectionManager().Tick();
    }
}

bool UEditorEngine::LoadScene(const FString &InSceneReference)
{
    return LevelEditor.GetPIEManager().LoadScene(InSceneReference);
}

UCameraComponent *UEditorEngine::GetCamera() const
{
    if (FLevelEditorViewportClient *ActiveVC = GetViewportLayout().GetActiveViewport())
    {
        return ActiveVC->GetCamera();
    }
    return nullptr;
}

bool UEditorEngine::FocusActorInViewport(AActor *Actor)
{
    if (FLevelEditorViewportClient *ActiveVC = GetViewportLayout().GetActiveViewport())
    {
        return ActiveVC->FocusActor(Actor);
    }
    return false;
}

void UEditorEngine::RenderUI(float DeltaTime) { MainFrame.Render(DeltaTime); }

void UEditorEngine::RenderPIEOverlayPopups() { LevelEditor.GetPIEManager().RenderOverlayPopups(); }

void UEditorEngine::OpenScoreSavePopup(int32 InScore) { LevelEditor.GetPIEManager().OpenScoreSavePopup(InScore); }

bool UEditorEngine::ConsumeScoreSavePopupResult(FString &OutNickname) { return LevelEditor.GetPIEManager().ConsumeScoreSavePopupResult(OutNickname); }

void UEditorEngine::OpenMessagePopup(const FString &InMessage) { LevelEditor.GetPIEManager().OpenMessagePopup(InMessage); }

bool UEditorEngine::ConsumeMessagePopupConfirmed() { return LevelEditor.GetPIEManager().ConsumeMessagePopupConfirmed(); }

void UEditorEngine::OpenScoreboardPopup(const FString &InFilePath) { LevelEditor.GetPIEManager().OpenScoreboardPopup(InFilePath); }

void UEditorEngine::OpenTitleOptionsPopup() { LevelEditor.GetPIEManager().OpenTitleOptionsPopup(); }

void UEditorEngine::OpenTitleCreditsPopup() { LevelEditor.GetPIEManager().OpenTitleCreditsPopup(); }

bool UEditorEngine::IsScoreSavePopupOpen() const { return LevelEditor.GetPIEManager().IsScoreSavePopupOpen(); }

void UEditorEngine::ToggleCoordSystem()
{
    FEditorSettings &Settings = FEditorSettings::Get();
    Settings.CoordSystem = (Settings.CoordSystem == EEditorCoordSystem::World) ? EEditorCoordSystem::Local : EEditorCoordSystem::World;
    ApplyTransformSettingsToGizmo();
}

void UEditorEngine::ApplyTransformSettingsToGizmo()
{
    UGizmoComponent *Gizmo = GetGizmo();
    if (!Gizmo)
    {
        return;
    }

    const FEditorSettings &Settings = FEditorSettings::Get();
    const bool             bForceLocalForScale = Gizmo->GetMode() == EGizmoMode::Scale;
    Gizmo->SetWorldSpace(bForceLocalForScale ? false : (Settings.CoordSystem == EEditorCoordSystem::World));
    // 에디터 설정의 좌표계/스냅 값을 매 프레임 Gizmo 상태와 동기화한다.
    Gizmo->SetSnapSettings(Settings.bEnableTranslationSnap, Settings.TranslationSnapSize, Settings.bEnableRotationSnap,
                           Settings.RotationSnapSize, Settings.bEnableScaleSnap, Settings.ScaleSnapSize);
}

void UEditorEngine::RequestPlaySession(const FRequestPlaySessionParams &InParams)
{
    LevelEditor.GetPIEManager().RequestPlaySession(InParams);
}

void UEditorEngine::CancelRequestPlaySession() { LevelEditor.GetPIEManager().CancelRequestPlaySession(); }

void UEditorEngine::RequestEndPlayMap() { LevelEditor.GetPIEManager().RequestEndPlayMap(); }

bool UEditorEngine::TogglePIEControlMode() { return LevelEditor.GetPIEManager().TogglePIEControlMode(); }

// ─── 기존 메서드 ──────────────────────────────────────────

void UEditorEngine::ResetViewport() { GetViewportLayout().ResetViewport(GetWorld()); }

void UEditorEngine::CloseScene() { SceneManager.CloseScene(); }

void UEditorEngine::NewScene() { SceneManager.NewScene(); }

void UEditorEngine::ClearScene() { SceneManager.ClearScene(); }

void UEditorEngine::BeginTrackedSceneChange()
{
    HistoryManager.BeginTrackedSceneChange();
}

void UEditorEngine::CommitTrackedSceneChange()
{
    HistoryManager.CommitTrackedSceneChange();
}

void UEditorEngine::CancelTrackedSceneChange()
{
    HistoryManager.CancelTrackedSceneChange();
}

bool UEditorEngine::CanUndoSceneChange() const
{
    return HistoryManager.CanUndoSceneChange();
}

bool UEditorEngine::CanRedoSceneChange() const { return HistoryManager.CanRedoSceneChange(); }

void UEditorEngine::UndoTrackedSceneChange()
{
    HistoryManager.UndoTrackedSceneChange();
}

void UEditorEngine::RedoTrackedSceneChange()
{
    HistoryManager.RedoTrackedSceneChange();
}

void UEditorEngine::ClearTrackedTransformHistory()
{
    HistoryManager.ClearTrackedTransformHistory();
}

void UEditorEngine::BeginTrackedTransformChange() { HistoryManager.BeginTrackedTransformChange(); }

void UEditorEngine::CommitTrackedTransformChange() { HistoryManager.CommitTrackedTransformChange(); }

bool UEditorEngine::CanUndoTransformChange() const { return HistoryManager.CanUndoTransformChange(); }

bool UEditorEngine::CanRedoTransformChange() const { return HistoryManager.CanRedoTransformChange(); }

void UEditorEngine::UndoTrackedTransformChange() { HistoryManager.UndoTrackedTransformChange(); }

void UEditorEngine::RedoTrackedTransformChange() { HistoryManager.RedoTrackedTransformChange(); }

void UEditorEngine::ApplyTrackedSceneChange(const FTrackedSceneChange &Change, bool bRedo)
{
    HistoryManager.ApplyTrackedSceneChange(Change, bRedo);
}

void UEditorEngine::ApplyTrackedActorDeltas(const FTrackedSceneChange &Change, bool bRedo)
{
    HistoryManager.ApplyTrackedActorDeltas(Change, bRedo);
}

void UEditorEngine::RestoreTrackedActorOrder(const TArray<uint32> &OrderedUUIDs)
{
    HistoryManager.RestoreTrackedActorOrder(OrderedUUIDs);
}

void UEditorEngine::RestoreTrackedFolderOrder(const TArray<FString> &OrderedFolders)
{
    HistoryManager.RestoreTrackedFolderOrder(OrderedFolders);
}

void UEditorEngine::RestoreTrackedSelection(const TArray<uint32> &SelectedUUIDs)
{
    HistoryManager.RestoreTrackedSelection(SelectedUUIDs);
}

void UEditorEngine::InvalidateTrackedSceneSnapshotCache() { HistoryManager.InvalidateTrackedSceneSnapshotCache(); }

UCameraComponent *UEditorEngine::FindSceneViewportCamera() const
{
    for (FLevelEditorViewportClient *VC : GetViewportLayout().GetLevelViewportClients())
    {
        if (!VC)
        {
            continue;
        }

        if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective ||
            VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
        {
            return VC->GetCamera();
        }
    }

    return nullptr;
}

void UEditorEngine::RestoreViewportCamera(const FPerspectiveCameraData &CamData)
{
    if (!CamData.bValid)
    {
        return;
    }

    if (UCameraComponent *Camera = FindSceneViewportCamera())
    {
        Camera->SetWorldLocation(CamData.Location);
        Camera->SetRelativeRotation(CamData.Rotation);
        FMinimalViewInfo CameraState = Camera->GetCameraState();
        CameraState.FOV = CamData.FOV;
        CameraState.NearZ = CamData.NearClip;
        CameraState.FarZ = CamData.FarClip;
        Camera->SetCameraState(CameraState);
    }
}

bool UEditorEngine::SaveSceneAs(const FString &InScenePath) { return SceneManager.SaveSceneAs(InScenePath); }

bool UEditorEngine::SaveScene() { return SceneManager.SaveScene(); }

void UEditorEngine::RequestSaveSceneAsDialog() { SceneManager.RequestSaveSceneAsDialog(); }

bool UEditorEngine::SaveSceneAsWithDialog() { return SceneManager.SaveSceneAsWithDialog(); }

bool UEditorEngine::LoadSceneFromPath(const FString &InScenePath) { return SceneManager.LoadSceneFromPath(InScenePath); }

bool UEditorEngine::LoadSceneWithDialog() { return SceneManager.LoadSceneWithDialog(); }

bool UEditorEngine::ImportMaterialWithDialog() { return AssetImportManager.ImportMaterialWithDialog(); }

bool UEditorEngine::ImportTextureWithDialog() { return AssetImportManager.ImportTextureWithDialog(); }
