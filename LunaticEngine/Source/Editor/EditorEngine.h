#pragma once

#include "AssetEditor/AssetEditorManager.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Serialization/SceneSaveManager.h"

#include "AssetTools/AssetImportManager.h"
#include "LevelEditor/LevelEditor.h"
#include "LevelEditor/PIE/LevelPIETypes.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"
#include "LevelEditor/Window/LevelEditorWindow.h"
#include "MainFrame/EditorMainFrame.h"

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
    bool           HasCurrentLevelFilePath() const { return LevelEditor.GetSceneManager().HasCurrentLevelFilePath(); }
    const FString &GetCurrentLevelFilePath() const { return LevelEditor.GetSceneManager().GetCurrentLevelFilePath(); }
    void           RefreshContentBrowser() { LevelEditorWindow.RefreshContentBrowser(); }
    void           SetContentBrowserIconSize(float Size) { LevelEditorWindow.SetContentBrowserIconSize(Size); }
    float          GetContentBrowserIconSize() const { return LevelEditorWindow.GetContentBrowserIconSize(); }
    void           HideEditorWindows() { LevelEditorWindow.HideEditorWindows(); }
    void           ShowEditorWindows() { LevelEditorWindow.ShowEditorWindows(); }
    void           SetShowEditorOnlyComponents(bool bEnable) { LevelEditorWindow.SetShowEditorOnlyComponents(bEnable); }
    bool           IsShowingEditorOnlyComponents() const { return LevelEditorWindow.IsShowingEditorOnlyComponents(); }
    bool           IsWorldCoordSystem() const { return FLevelEditorSettings::Get().CoordSystem == EEditorCoordSystem::World; }
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

    FLevelEditorSettings       &GetSettings() { return FLevelEditorSettings::Get(); }
    const FLevelEditorSettings &GetSettings() const { return FLevelEditorSettings::Get(); }

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
    void RequestPlaySession(const FRequestPlaySessionParams &InParams);
    void CancelRequestPlaySession();
    bool HasPlaySessionRequest() const { return LevelEditor.GetPIEManager().HasPlaySessionRequest(); }

    void RequestEndPlayMap();
    bool IsPlayingInEditor() const { return LevelEditor.GetPIEManager().IsPlayingInEditor(); }
    EPIEControlMode GetPIEControlMode() const { return LevelEditor.GetPIEManager().GetPIEControlMode(); }
    bool            IsPIEPossessedMode() const { return LevelEditor.GetPIEManager().IsPIEPossessedMode(); }
    bool            IsPIEEjectedMode() const { return LevelEditor.GetPIEManager().IsPIEEjectedMode(); }
    bool            TogglePIEControlMode();

    // 즉시 동기 종료 — Save / NewScene / Load 등 에디터 월드를 만지는 작업 직전에 호출.
    // PIE 중이 아니면 no-op.
    void StopPlayInEditorImmediate()
    {
        LevelEditor.GetPIEManager().StopPlayInEditorImmediate();
    }

    bool OpenAssetFromPath(const std::filesystem::path &AssetPath)
    {
        return AssetEditorManager.OpenAssetFromPath(AssetPath);
    }

    bool IsAssetEditorCapturingInput() const { return AssetEditorManager.IsCapturingInput(); }
    FAssetEditorManager &GetAssetEditorManager() { return AssetEditorManager; }
    const FAssetEditorManager &GetAssetEditorManager() const { return AssetEditorManager; }

  private:
    friend class FLevelSceneManager;
    friend class FLevelEditorHistoryManager;
    friend class FLevelPIEManager;

    UCameraComponent *FindSceneViewportCamera() const;
    void              RestoreViewportCamera(const FPerspectiveCameraData &CamData);
    void              ClearTrackedTransformHistory();
    void              ApplyTrackedSceneChange(const FTrackedSceneChange &Change, bool bRedo);
    void              ApplyTrackedActorDeltas(const FTrackedSceneChange &Change, bool bRedo);
    void              RestoreTrackedActorOrder(const TArray<uint32> &OrderedUUIDs);
    void              RestoreTrackedFolderOrder(const TArray<FString> &OrderedFolders);
    void              RestoreTrackedSelection(const TArray<uint32> &SelectedUUIDs);
    void              InvalidateTrackedSceneSnapshotCache();

    FLevelEditor       LevelEditor;
    FEditorMainFrame   MainFrame;
    FLevelEditorWindow LevelEditorWindow;
    FAssetEditorManager AssetEditorManager;
    FAssetImportManager AssetImportManager;
};
