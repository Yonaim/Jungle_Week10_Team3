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
    FLevelEditorSettings::Get().LoadFromFile(FLevelEditorSettings::GetDefaultSettingsPath());
    FProjectSettings::Get().LoadFromFile(FProjectSettings::GetDefaultPath());

    AssetEditorManager.Initialize(this, &Renderer);
    AssetImportManager.Init(this);

    // 기본 월드 생성 — 모든 서브시스템 초기화의 기반
    CreateWorldContext(EWorldType::Editor, FName("Default"));
    SetActiveWorld(WorldList[0].ContextHandle);
    GetWorld()->InitWorld();

    {
        SCOPE_STARTUP_STAT("LevelEditor::Initialize");
        LevelEditor.Initialize(this, Window, Renderer);
    }

    MainFrame.Create(Window, Renderer, this);
    LevelEditorWindow.Create(Window, Renderer, this, &LevelEditor, &MainFrame);

    {
        SCOPE_STARTUP_STAT("Editor::LoadStartLevel");
        LevelEditor.GetSceneManager().LoadStartLevel();
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
    LevelEditorWindow.SaveToSettings();
    FProjectSettings::Get().SaveToFile(FProjectSettings::GetDefaultPath());
    FLevelEditorSettings::Get().SaveToFile(FLevelEditorSettings::GetDefaultSettingsPath());
    CloseScene();
    FDirectoryWatcher::Get().Shutdown();
    AssetEditorManager.Shutdown();
    AssetImportManager.Shutdown();
    LevelEditor.Shutdown();
    LevelEditorWindow.Release();
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
    LevelEditor.Tick(DeltaTime);

    ApplyTransformSettingsToGizmo();
    FDirectoryWatcher::Get().ProcessChanges();
    if (UTexture2D::HasPendingTextureRefresh())
    {
        UTexture2D::RefreshChangedTextures(Renderer.GetFD3DDevice().GetDevice());
    }
    FNotificationManager::Get().Tick(DeltaTime);
    FAudioManager::Get().Update();
    AssetEditorManager.Tick(DeltaTime);
    LevelEditorWindow.Update();
    MainFrame.UpdateInputState(IsMouseOverViewport(), AssetEditorManager.IsCapturingInput(), IsScoreSavePopupOpen());

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

void UEditorEngine::RenderUI(float DeltaTime)
{
    MainFrame.BeginFrame();
    MainFrame.RenderMainMenuBar(&LevelEditorWindow);
    LevelEditorWindow.FlushPendingMenuAction();
    LevelEditorWindow.Render(DeltaTime);
    MainFrame.RenderCommonOverlays();
    MainFrame.EndFrame();

    AssetEditorManager.Render(DeltaTime);
}

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
    FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
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

    const FLevelEditorSettings &Settings = FLevelEditorSettings::Get();
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

void UEditorEngine::CloseScene() { LevelEditor.GetSceneManager().CloseScene(); }

void UEditorEngine::NewScene() { LevelEditor.GetSceneManager().NewScene(); }

void UEditorEngine::ClearScene() { LevelEditor.GetSceneManager().ClearScene(); }

void UEditorEngine::BeginTrackedSceneChange()
{
    LevelEditor.GetHistoryManager().BeginTrackedSceneChange();
}

void UEditorEngine::CommitTrackedSceneChange()
{
    LevelEditor.GetHistoryManager().CommitTrackedSceneChange();
}

void UEditorEngine::CancelTrackedSceneChange()
{
    LevelEditor.GetHistoryManager().CancelTrackedSceneChange();
}

bool UEditorEngine::CanUndoSceneChange() const
{
    return LevelEditor.GetHistoryManager().CanUndoSceneChange();
}

bool UEditorEngine::CanRedoSceneChange() const { return LevelEditor.GetHistoryManager().CanRedoSceneChange(); }

void UEditorEngine::UndoTrackedSceneChange()
{
    LevelEditor.GetHistoryManager().UndoTrackedSceneChange();
}

void UEditorEngine::RedoTrackedSceneChange()
{
    LevelEditor.GetHistoryManager().RedoTrackedSceneChange();
}

void UEditorEngine::ClearTrackedTransformHistory()
{
    LevelEditor.GetHistoryManager().ClearTrackedTransformHistory();
}

void UEditorEngine::BeginTrackedTransformChange() { LevelEditor.GetHistoryManager().BeginTrackedTransformChange(); }

void UEditorEngine::CommitTrackedTransformChange() { LevelEditor.GetHistoryManager().CommitTrackedTransformChange(); }

bool UEditorEngine::CanUndoTransformChange() const { return LevelEditor.GetHistoryManager().CanUndoTransformChange(); }

bool UEditorEngine::CanRedoTransformChange() const { return LevelEditor.GetHistoryManager().CanRedoTransformChange(); }

void UEditorEngine::UndoTrackedTransformChange() { LevelEditor.GetHistoryManager().UndoTrackedTransformChange(); }

void UEditorEngine::RedoTrackedTransformChange() { LevelEditor.GetHistoryManager().RedoTrackedTransformChange(); }

void UEditorEngine::ApplyTrackedSceneChange(const FTrackedSceneChange &Change, bool bRedo)
{
    LevelEditor.GetHistoryManager().ApplyTrackedSceneChange(Change, bRedo);
}

void UEditorEngine::ApplyTrackedActorDeltas(const FTrackedSceneChange &Change, bool bRedo)
{
    LevelEditor.GetHistoryManager().ApplyTrackedActorDeltas(Change, bRedo);
}

void UEditorEngine::RestoreTrackedActorOrder(const TArray<uint32> &OrderedUUIDs)
{
    LevelEditor.GetHistoryManager().RestoreTrackedActorOrder(OrderedUUIDs);
}

void UEditorEngine::RestoreTrackedFolderOrder(const TArray<FString> &OrderedFolders)
{
    LevelEditor.GetHistoryManager().RestoreTrackedFolderOrder(OrderedFolders);
}

void UEditorEngine::RestoreTrackedSelection(const TArray<uint32> &SelectedUUIDs)
{
    LevelEditor.GetHistoryManager().RestoreTrackedSelection(SelectedUUIDs);
}

void UEditorEngine::InvalidateTrackedSceneSnapshotCache() { LevelEditor.GetHistoryManager().InvalidateTrackedSceneSnapshotCache(); }

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

bool UEditorEngine::SaveSceneAs(const FString &InScenePath) { return LevelEditor.GetSceneManager().SaveSceneAs(InScenePath); }

bool UEditorEngine::SaveScene() { return LevelEditor.GetSceneManager().SaveScene(); }

void UEditorEngine::RequestSaveSceneAsDialog() { LevelEditor.GetSceneManager().RequestSaveSceneAsDialog(); }

bool UEditorEngine::SaveSceneAsWithDialog() { return LevelEditor.GetSceneManager().SaveSceneAsWithDialog(); }

bool UEditorEngine::LoadSceneFromPath(const FString &InScenePath) { return LevelEditor.GetSceneManager().LoadSceneFromPath(InScenePath); }

bool UEditorEngine::LoadSceneWithDialog() { return LevelEditor.GetSceneManager().LoadSceneWithDialog(); }

bool UEditorEngine::ImportMaterialWithDialog() { return AssetImportManager.ImportMaterialWithDialog(); }

bool UEditorEngine::ImportTextureWithDialog() { return AssetImportManager.ImportTextureWithDialog(); }
