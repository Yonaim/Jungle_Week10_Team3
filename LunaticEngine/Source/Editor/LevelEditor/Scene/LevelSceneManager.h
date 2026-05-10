#pragma once

#include "Core/CoreTypes.h"

class UEditorEngine;

class FLevelSceneManager
{
public:
    void Init(UEditorEngine* InEditorEngine);
    void Shutdown();

    void Tick(float DeltaTime);

    void CloseScene();
    void NewScene();
    void ClearScene();

    void LoadStartLevel();

    bool LoadSceneFromPath(const FString& InScenePath);
    bool LoadSceneWithDialog();

    bool SaveScene();
    bool SaveSceneAs(const FString& InScenePath);
    bool SaveSceneAsWithDialog();
    void RequestSaveSceneAsDialog();

    bool HasCurrentLevelFilePath() const
    {
        return !CurrentLevelFilePath.empty();
    }

    const FString& GetCurrentLevelFilePath() const
    {
        return CurrentLevelFilePath;
    }

private:
    void ProcessDeferredActions();
    void DestroyCurrentSceneWorlds(bool bClearHistory, bool bResetLevelPath);

private:
    UEditorEngine* EditorEngine = nullptr;
    bool bRequestSaveSceneAsDialogQueued = false;
    FString CurrentLevelFilePath;
};
