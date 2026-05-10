#pragma once

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/GameImGuiOverlay.h"
#include "Engine/Serialization/SceneSaveManager.h"

#include "LevelEditor/History/SceneHistoryTypes.h"
#include "LevelEditor/History/LevelEditorHistoryManager.h"
#include "LevelEditor/LevelEditor.h"
#include "MainFrame/EditorMainFrame.h"
#include "PIE/PIETypes.h"
#include "Scene/EditorSceneManager.h"
#include "Settings/EditorSettings.h"
#include <optional>

#if STATS
#include "LevelEditor/Render/EditorRenderPipeline.h"
#endif

class UGizmoComponent;
class FLevelEditorViewportClient;
class FEditorViewportClient;
class FOverlayStatSystem;
class AActor;
class UGameViewportClient;
struct FPerspectiveCameraData;

class UEditorEngine : public UEngine
{
  public:
    DECLARE_CLASS(UEditorEngine, UEngine)

    UEditorEngine() = default;
    ~UEditorEngine() override = default;

    // Lifecycle overrides
    void Init(FWindowsWindow *InWindow) override;
    void Shutdown() override;
    void Tick(float DeltaTime) override;
    void OnWindowResized(uint32 Width, uint32 Height) override;
    bool LoadScene(const FString &InSceneReference) override;
    void OpenScoreSavePopup(int32 InScore) override;
    bool ConsumeScoreSavePopupResult(FString &OutNickname) override;
    void OpenMessagePopup(const FString &InMessage) override;
    bool ConsumeMessagePopupConfirmed() override;
    void OpenScoreboardPopup(const FString &InFilePath) override;
    void OpenTitleOptionsPopup() override;
    void OpenTitleCreditsPopup() override;
    bool IsScoreSavePopupOpen() const override;

    // Editor-specific API
    UGizmoComponent  *GetGizmo() const { return GetSelectionManager().GetGizmo(); }
    UCameraComponent *GetCamera() const;
    bool              FocusActorInViewport(AActor *Actor);

    void           ClearScene();
    void           ResetViewport();
    void           CloseScene();
    void           NewScene();
    bool           LoadSceneWithDialog();
    bool           LoadSceneFromPath(const FString &InScenePath);
    bool           ImportMaterialWithDialog();
    bool           ImportTextureWithDialog();
    bool           SaveScene();
    void           RequestSaveSceneAsDialog();
    bool           SaveSceneAsWithDialog();
    bool           SaveSceneAs(const FString &InScenePath);
    bool           HasCurrentLevelFilePath() const { return SceneManager.HasCurrentLevelFilePath(); }
    const FString &GetCurrentLevelFilePath() const { return SceneManager.GetCurrentLevelFilePath(); }
    void           RefreshContentBrowser() { MainFrame.RefreshContentBrowser(); }
    void           SetContentBrowserIconSize(float Size) { MainFrame.SetContentBrowserIconSize(Size); }
    float          GetContentBrowserIconSize() const { return MainFrame.GetContentBrowserIconSize(); }
    void           HideEditorWindows() { MainFrame.HideEditorWindows(); }
    void           ShowEditorWindows() { MainFrame.ShowEditorWindows(); }
    void           SetShowEditorOnlyComponents(bool bEnable) { MainFrame.SetShowEditorOnlyComponents(bEnable); }
    bool           IsShowingEditorOnlyComponents() const { return MainFrame.IsShowingEditorOnlyComponents(); }
    bool           IsWorldCoordSystem() const { return FEditorSettings::Get().CoordSystem == EEditorCoordSystem::World; }
    void           ToggleCoordSystem();
    void           ApplyTransformSettingsToGizmo();
    void           BeginTrackedSceneChange();
    void           CommitTrackedSceneChange();
    void           CancelTrackedSceneChange();
    bool           CanUndoSceneChange() const;
    bool           CanRedoSceneChange() const;
    void           UndoTrackedSceneChange();
    void           RedoTrackedSceneChange();
    void           BeginTrackedTransformChange();
    void           CommitTrackedTransformChange();
    bool           CanUndoTransformChange() const;
    bool           CanRedoTransformChange() const;
    void           UndoTrackedTransformChange();
    void           RedoTrackedTransformChange();

    // GPU Occlusion readback 스테이징 데이터 무효화 — 액터 삭제 시 dangling proxy 방지
    void InvalidateOcclusionResults()
    {
        if (auto *P = GetRenderPipeline())
            P->OnSceneCleared();
    }

    FEditorSettings       &GetSettings() { return FEditorSettings::Get(); }
    const FEditorSettings &GetSettings() const { return FEditorSettings::Get(); }

    FSelectionManager       &GetSelectionManager() { return LevelEditor.GetSelectionManager(); }
    const FSelectionManager &GetSelectionManager() const { return LevelEditor.GetSelectionManager(); }

    FLevelViewportLayout       &GetViewportLayout() { return LevelEditor.GetViewportLayout(); }
    const FLevelViewportLayout &GetViewportLayout() const { return LevelEditor.GetViewportLayout(); }
    // 레이아웃에 위임
    const TArray<FEditorViewportClient *> &GetAllViewportClients() const { return LevelEditor.GetViewportLayout().GetAllViewportClients(); }
    const TArray<FLevelEditorViewportClient *> &GetLevelViewportClients() const
    {
        return LevelEditor.GetViewportLayout().GetLevelViewportClients();
    }
    bool ShouldRenderViewportClient(const FLevelEditorViewportClient *ViewportClient) const
    {
        return LevelEditor.GetViewportLayout().ShouldRenderViewportClient(ViewportClient);
    }

    void SetActiveViewport(FLevelEditorViewportClient *InClient) { LevelEditor.GetViewportLayout().SetActiveViewport(InClient); }
    FLevelEditorViewportClient *GetActiveViewport() const { return LevelEditor.GetViewportLayout().GetActiveViewport(); }

    void ToggleViewportSplit() { LevelEditor.GetViewportLayout().ToggleViewportSplit(); }
    bool IsSplitViewport() const { return LevelEditor.GetViewportLayout().IsSplitViewport(); }

    void    RenderViewportUI(float DeltaTime) { LevelEditor.GetViewportLayout().RenderViewportUI(DeltaTime); }
    AActor *SpawnPlaceActor(FLevelViewportLayout::EViewportPlaceActorType Type, const FVector &Location)
    {
        return LevelEditor.GetViewportLayout().SpawnPlaceActor(Type, Location);
    }

    bool IsMouseOverViewport() const { return LevelEditor.GetViewportLayout().IsMouseOverViewport(); }

    void RenderUI(float DeltaTime);
    void RenderPIEOverlayPopups();

    FOverlayStatSystem       &GetOverlayStatSystem() { return LevelEditor.GetOverlayStatSystem(); }
    const FOverlayStatSystem &GetOverlayStatSystem() const { return LevelEditor.GetOverlayStatSystem(); }

    // --- PIE (Play In Editor) ---
    // UE의 FRequestPlaySessionParams 대응. 요청은 단일 슬롯에 저장되고
    // 다음 Tick에서 StartQueuedPlaySessionRequest가 실제 StartPIE를 수행한다.
    void RequestPlaySession(const FRequestPlaySessionParams &InParams);
    void CancelRequestPlaySession();
    bool HasPlaySessionRequest() const { return PlaySessionRequest.has_value(); }

    void RequestEndPlayMap();
    bool IsPlayingInEditor() const { return PlayInEditorSessionInfo.has_value(); }
    enum class EPIEControlMode : uint8
    {
        Possessed,
        Ejected
    };
    EPIEControlMode GetPIEControlMode() const { return PIEControlMode; }
    bool            IsPIEPossessedMode() const { return IsPlayingInEditor() && PIEControlMode == EPIEControlMode::Possessed; }
    bool            IsPIEEjectedMode() const { return IsPlayingInEditor() && PIEControlMode == EPIEControlMode::Ejected; }
    bool            TogglePIEControlMode();

    // 즉시 동기 종료 — Save / NewScene / Load 등 에디터 월드를 만지는 작업 직전에 호출.
    // PIE 중이 아니면 no-op.
    void StopPlayInEditorImmediate()
    {
        if (IsPlayingInEditor())
            EndPlayMap();
    }

    bool OpenAssetFromPath(const std::filesystem::path &AssetPath)
    {
        return MainFrame.GetAssetEditorManager().OpenAssetFromPath(AssetPath);
    }

    bool IsAssetEditorCapturingInput() const { return MainFrame.GetAssetEditorManager().IsCapturingInput(); }

  private:
    friend class FEditorSceneManager;
    friend class FLevelEditorHistoryManager;

    // Tick 내에서 호출 — 큐에 요청이 있으면 StartPlayInEditorSession 실행
    void              StartQueuedPlaySessionRequest();
    void              StartPlayInEditorSession(const FRequestPlaySessionParams &Params);
    void              EndPlayMap();
    bool              EnterPIEPossessedMode();
    bool              EnterPIEEjectedMode();
    void              SyncGameViewportPIEControlState(bool bPossessedMode);
    UCameraComponent *FindSceneViewportCamera() const;
    void              RestoreViewportCamera(const FPerspectiveCameraData &CamData);
    void              ClearTrackedTransformHistory();
    void              ApplyTrackedSceneChange(const FTrackedSceneChange &Change, bool bRedo);
    void              ApplyTrackedActorDeltas(const FTrackedSceneChange &Change, bool bRedo);
    void              RestoreTrackedActorOrder(const TArray<uint32> &OrderedUUIDs);
    void              RestoreTrackedFolderOrder(const TArray<FString> &OrderedFolders);
    void              RestoreTrackedSelection(const TArray<uint32> &SelectedUUIDs);
    void              InvalidateTrackedSceneSnapshotCache();

    FLevelEditor     LevelEditor;
    FEditorMainFrame MainFrame;
    FEditorSceneManager SceneManager;
    FLevelEditorHistoryManager HistoryManager;

    // PIE 요청 단일 슬롯 (UE TOptional<FRequestPlaySessionParams>).
    std::optional<FRequestPlaySessionParams> PlaySessionRequest;
    // 활성 PIE 세션 정보. has_value() == IsPlayingInEditor().
    std::optional<FPlayInEditorSessionInfo> PlayInEditorSessionInfo;
    // 종료 요청 지연 플래그. Tick 선두에서 확인 후 EndPlayMap 호출.
    bool                                 bRequestEndPlayMapQueued = false;
    EPIEControlMode                      PIEControlMode = EPIEControlMode::Possessed;
    FGameImGuiOverlay                    PIEOverlay;
};
