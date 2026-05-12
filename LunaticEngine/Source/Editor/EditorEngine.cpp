#include "PCH/LunaticPCH.h"
#include "EditorEngine.h"
#include "Common/UI/Panels/PanelTitleUtils.h"

#include "Audio/AudioManager.h"
#include "Common/File/EditorFileUtils.h"
#include "Component/CameraComponent.h"
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
    // 사용자 설정에서 카메라/경로 정보는 가져오되, Editor 레이아웃과 패널 상태는 매 실행마다 기본값으로 시작한다.
    FLevelEditorSettings::Get().ResetEditorLayoutToDefault();
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

    ImGuiSystem.Initialize(Window, &Renderer.GetFD3DDevice());
    LevelEditorWindow.Create(Window, Renderer, this, &LevelEditor);

    {
        SCOPE_STARTUP_STAT("Editor::LoadStartLevel");
        LevelEditor.GetSceneManager().LoadStartLevel();
    }
    ApplyTransformSettingsToGizmo();

    // 에디터 렌더 파이프라인
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
    ImGuiSystem.Shutdown();

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
    // Asset Editor 컨텍스트가 활성화되어 있어도 Level 월드와 에디터 서브시스템은 계속 Tick된다.
    // 활성 컨텍스트는 뷰포트 입력/렌더 포커스만 가지며, 에디터 월드 자체를 멈추지는 않는다.
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

    // 입력 캡처 여부는 보이는 모든 패널이 아니라 현재 활성 에디터 컨텍스트가 결정한다.
    // 이렇게 하면 Asset Editor 패널이 Level Viewport 입력을 가로채지 않고,
    // 커서가 Asset Preview Viewport 위에 있을 때는 해당 뷰포트가 ImGui 캡처를 적절히 해제할 수 있다.
    LevelEditorWindow.UpdateInputState(IsMouseOverActiveViewport(), false, IsScoreSavePopupOpen());

    if (IsLevelEditorContextActive())
    {
        for (FEditorViewportClient *VC : GetViewportLayout().GetAllViewportClients())
        {
            VC->Tick(DeltaTime);
        }
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

FEditorViewportCamera *UEditorEngine::GetCamera() const
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

void UEditorEngine::SetActiveEditorContext(EEditorContextType InContextType)
{
    if (ActiveEditorContextType == InContextType)
    {
        if (InContextType == EEditorContextType::AssetEditor)
        {
            HideLevelEditorUIForAssetEditor();
        }
        else
        {
            RestoreLevelEditorUIAfterAssetEditor();
            LevelEditorWindow.RequestDefaultDockLayout();
        }
        return;
    }

    ActiveEditorContextType = InContextType;

    if (IsAssetEditorContextActive())
    {
        HideLevelEditorUIForAssetEditor();
        if (FEditorViewportClient *ViewportClient = AssetEditorManager.GetActiveViewportClient())
        {
            ViewportClient->SetActive(true);
        }
    }
    else
    {
        RestoreLevelEditorUIAfterAssetEditor();
        LevelEditorWindow.RequestDefaultDockLayout();
        if (FLevelEditorViewportClient *LevelViewportClient = GetActiveViewport())
        {
            LevelViewportClient->SetActive(true);
        }
    }
}

FEditorViewportClient *UEditorEngine::GetActiveEditorViewportClient() const
{
    if (IsAssetEditorContextActive())
    {
        return AssetEditorManager.GetActiveViewportClient();
    }

    return GetViewportLayout().GetActiveViewport();
}

bool UEditorEngine::IsMouseOverActiveViewport() const
{
    if (IsAssetEditorContextActive())
    {
        if (FEditorViewportClient *ViewportClient = AssetEditorManager.GetActiveViewportClient())
        {
            return ViewportClient->IsHovered();
        }
        return false;
    }

    return IsMouseOverViewport();
}

void UEditorEngine::RenderUI(float DeltaTime)
{
    ImGuiSystem.BeginFrame();

    LevelEditorWindow.RenderContent(DeltaTime);
    if (IsAssetEditorContextActive())
    {
        AssetEditorManager.RenderContent(DeltaTime, LevelEditorWindow.GetMainDockspaceId());
    }

    // Level Editor 패널과 Asset/SkeletalMesh Editor 패널을 모두 렌더링한 뒤 한 번만 flush한다.
    // PanelTitleUtils는 이 시점에 dock tab bar의 빈 영역 fill과 선택 탭 accent line을 그린다.
    PanelTitleUtils::FlushPanelDecorations();

    ImGuiSystem.EndFrame();
    LevelEditorWindow.FlushPendingMenuAction();
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

void UEditorEngine::SetEditorGizmoMode(EGizmoMode NewMode)
{
    EditorGizmoSharedState.Mode = NewMode;
    ApplyTransformSettingsToGizmo();
}

void UEditorEngine::ApplyTransformSettingsToGizmo()
{
    const FLevelEditorSettings &Settings = FLevelEditorSettings::Get();

    const bool bForceLocalForScale = EditorGizmoSharedState.Mode == EGizmoMode::Scale;
    EditorGizmoSharedState.Space =
        bForceLocalForScale || Settings.CoordSystem != EEditorCoordSystem::World
            ? EGizmoSpace::Local
            : EGizmoSpace::World;

    EditorGizmoSharedState.bTranslationSnapEnabled = Settings.bEnableTranslationSnap;
    EditorGizmoSharedState.TranslationSnapSize = Settings.TranslationSnapSize;
    EditorGizmoSharedState.bRotationSnapEnabled = Settings.bEnableRotationSnap;
    EditorGizmoSharedState.RotationSnapSizeDegrees = Settings.RotationSnapSize;
    EditorGizmoSharedState.bScaleSnapEnabled = Settings.bEnableScaleSnap;
    EditorGizmoSharedState.ScaleSnapSize = Settings.ScaleSnapSize;

    // The tool state is editor-context-wide. Each viewport still owns its own
    // FGizmoManager / visual / drag state, but mode/space/snap must be identical
    // across all visible Level viewport managers.
    if (IsLevelEditorContextActive())
    {
        for (FLevelEditorViewportClient* Client : GetLevelViewportClients())
        {
            if (Client)
            {
                EditorGizmoSharedState.ApplyTo(Client->GetGizmoManager());
            }
        }
        return;
    }

    // Asset editor viewport clients use their own editor state where available.
    // For generic asset viewports, apply the current shared editor tool state to
    // every collected asset viewport instead of only the active one.
    TArray<FEditorViewportClient*> AssetClients;
    CollectAssetViewportClients(AssetClients);
    for (FEditorViewportClient* Client : AssetClients)
    {
        if (Client)
        {
            EditorGizmoSharedState.ApplyTo(Client->GetGizmoManager());
        }
    }
}

void UEditorEngine::SyncActiveGizmoVisualFromTarget()
{
    if (IsLevelEditorContextActive())
    {
        for (FLevelEditorViewportClient* Client : GetLevelViewportClients())
        {
            if (Client)
            {
                Client->GetGizmoManager().SyncVisualFromTarget();
            }
        }
        return;
    }

    TArray<FEditorViewportClient*> AssetClients;
    CollectAssetViewportClients(AssetClients);
    for (FEditorViewportClient* Client : AssetClients)
    {
        if (Client)
        {
            Client->GetGizmoManager().SyncVisualFromTarget();
        }
    }
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

FEditorViewportCamera *UEditorEngine::FindSceneViewportCamera() const
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

    if (FEditorViewportCamera *Camera = FindSceneViewportCamera())
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
