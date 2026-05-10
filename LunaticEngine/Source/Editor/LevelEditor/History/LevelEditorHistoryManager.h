#pragma once

#include "LevelEditor/History/SceneHistoryTypes.h"
#include <optional>

class UEditorEngine;

class FLevelEditorHistoryManager
{
public:
    void Init(UEditorEngine* InEditorEngine);
    void Shutdown();

    void BeginTrackedSceneChange();
    void CommitTrackedSceneChange();
    void CancelTrackedSceneChange();

    bool CanUndoSceneChange() const;
    bool CanRedoSceneChange() const;
    void UndoTrackedSceneChange();
    void RedoTrackedSceneChange();

    void ClearTrackedTransformHistory();

    void BeginTrackedTransformChange();
    void CommitTrackedTransformChange();
    bool CanUndoTransformChange() const;
    bool CanRedoTransformChange() const;
    void UndoTrackedTransformChange();
    void RedoTrackedTransformChange();

    void ApplyTrackedSceneChange(const FTrackedSceneChange& Change, bool bRedo);
    void ApplyTrackedActorDeltas(const FTrackedSceneChange& Change, bool bRedo);
    void RestoreTrackedActorOrder(const TArray<uint32>& OrderedUUIDs);
    void RestoreTrackedFolderOrder(const TArray<FString>& OrderedFolders);
    void RestoreTrackedSelection(const TArray<uint32>& SelectedUUIDs);
    void InvalidateTrackedSceneSnapshotCache();

private:
    UEditorEngine* EditorEngine = nullptr;

    TArray<FTrackedSceneChange> SceneHistory;
    int32 SceneHistoryCursor = -1;
    std::optional<FTrackedSceneSnapshot> PendingTrackedSceneBefore;
    std::optional<FTrackedSceneSnapshot> CachedTrackedSceneSnapshot;
    bool bTrackingSceneChange = false;
};
