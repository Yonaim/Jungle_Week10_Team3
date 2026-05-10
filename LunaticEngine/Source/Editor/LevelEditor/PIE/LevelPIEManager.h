#pragma once

#include "Engine/Runtime/GameImGuiOverlay.h"
#include "LevelEditor/PIE/LevelPIETypes.h"
#include <optional>

class UEditorEngine;

class FLevelPIEManager
{
public:
    void Init(UEditorEngine* InEditorEngine);
    void Shutdown();
    void Tick(float DeltaTime);

    bool LoadScene(const FString& InSceneReference);

    void RenderOverlayPopups();

    void RequestPlaySession(const FRequestPlaySessionParams& InParams);
    void CancelRequestPlaySession();
    bool HasPlaySessionRequest() const;

    void RequestEndPlayMap();
    bool IsPlayingInEditor() const;
    EPIEControlMode GetPIEControlMode() const;
    bool IsPIEPossessedMode() const;
    bool IsPIEEjectedMode() const;
    bool TogglePIEControlMode();
    void StopPlayInEditorImmediate();

    void OpenScoreSavePopup(int32 InScore);
    bool ConsumeScoreSavePopupResult(FString& OutNickname);
    void OpenMessagePopup(const FString& InMessage);
    bool ConsumeMessagePopupConfirmed();
    void OpenScoreboardPopup(const FString& InFilePath);
    void OpenTitleOptionsPopup();
    void OpenTitleCreditsPopup();
    bool IsScoreSavePopupOpen() const;

private:
    void StartQueuedPlaySessionRequest();
    void StartPlayInEditorSession(const FRequestPlaySessionParams& Params);
    void EndPlayMap();
    bool EnterPIEPossessedMode();
    bool EnterPIEEjectedMode();
    void SyncGameViewportPIEControlState(bool bPossessedMode);

private:
    UEditorEngine* EditorEngine = nullptr;
    std::optional<FRequestPlaySessionParams> PlaySessionRequest;
    std::optional<FPlayInEditorSessionInfo> PlayInEditorSessionInfo;
    bool bRequestEndPlayMapQueued = false;
    EPIEControlMode PIEControlMode = EPIEControlMode::Possessed;
    FGameImGuiOverlay PIEOverlay;
};
