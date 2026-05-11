#include "LevelEditor/PIE/LevelPIEManager.h"

#include "Audio/AudioManager.h"
#include "Component/CameraComponent.h"
#include "Core/AsciiUtils.h"
#include "Core/Notification.h"
#include "EditorEngine.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PawnActor.h"
#include "GameFramework/World.h"
#include "LevelEditor/Selection/SelectionManager.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "Object/Object.h"
#include "Viewport/GameViewportClient.h"
#include <filesystem>

namespace
{
    bool EndsWithIgnoreCase(const FString& Value, const char* Suffix)
    {
        if (!Suffix)
        {
            return false;
        }

        const FString SuffixString = Suffix;
        if (Value.size() < SuffixString.size())
        {
            return false;
        }

        for (size_t Index = 0; Index < SuffixString.size(); ++Index)
        {
            const char Left = AsciiUtils::ToLower(Value[Value.size() - SuffixString.size() + Index]);
            const char Right = AsciiUtils::ToLower(SuffixString[Index]);
            if (Left != Right)
            {
                return false;
            }
        }

        return true;
    }

    UCameraComponent* FindFirstCameraComponent(UWorld* World)
    {
        if (!World)
        {
            return nullptr;
        }

        for (AActor* Actor : World->GetActors())
        {
            if (!Actor)
            {
                continue;
            }

            for (UActorComponent* Comp : Actor->GetComponents())
            {
                if (UCameraComponent* Camera = Cast<UCameraComponent>(Comp))
                {
                    return Camera;
                }
            }
        }

        return nullptr;
    }

    UCameraComponent* EnsurePIEActiveCamera(UWorld* World, const FPerspectiveCameraData& CameraData)
    {
        if (!World)
        {
            return nullptr;
        }

        if (UCameraComponent* ActiveCamera = World->GetActiveCamera())
        {
            if (IsAliveObject(ActiveCamera) && ActiveCamera->GetWorld() == World)
            {
                return ActiveCamera;
            }
            World->SetActiveCamera(nullptr);
        }

        if (UCameraComponent* SceneCamera = FindFirstCameraComponent(World))
        {
            World->SetActiveCamera(SceneCamera);
            return SceneCamera;
        }

        AActor* CamActor = World->SpawnActor<AActor>();
        if (!CamActor)
        {
            return nullptr;
        }

        CamActor->SetFName(FName("DefaultPIECamera"));
        UCameraComponent* Cam = CamActor->AddComponent<UCameraComponent>();
        CamActor->SetRootComponent(Cam);
        if (CameraData.bValid)
        {
            Cam->SetRelativeLocation(CameraData.Location);
            Cam->SetRelativeRotation(CameraData.Rotation);
        }
        else
        {
            Cam->SetRelativeLocation(FVector(0.0f, -10.0f, 5.0f));
            Cam->SetRelativeRotation(FVector(0.0f, -25.0f, 90.0f));
        }

        World->SetActiveCamera(Cam);
        return Cam;
    }

    bool HasGameplayCamera(UWorld* World)
    {
        if (!World)
        {
            return false;
        }

        AGameModeBase* GameMode = World->GetAuthGameMode();
        APawnActor* SpawnedPawn = GameMode ? GameMode->GetSpawnedPawn() : nullptr;
        if (!SpawnedPawn)
        {
            return false;
        }

        for (UActorComponent* Comp : SpawnedPawn->GetComponents())
        {
            if (Cast<UCameraComponent>(Comp))
            {
                return true;
            }
        }

        return false;
    }
}

void FLevelPIEManager::Init(UEditorEngine* InEditorEngine)
{
    EditorEngine = InEditorEngine;
}

void FLevelPIEManager::Shutdown()
{
    StopPlayInEditorImmediate();
    PlaySessionRequest.reset();
    PlayInEditorSessionInfo.reset();
    bRequestEndPlayMapQueued = false;
    PIEControlMode = EPIEControlMode::Possessed;
    EditorEngine = nullptr;
}

void FLevelPIEManager::Tick(float DeltaTime)
{
    (void)DeltaTime;

    if (bRequestEndPlayMapQueued)
    {
        bRequestEndPlayMapQueued = false;
        EndPlayMap();
    }

    if (PlaySessionRequest.has_value())
    {
        StartQueuedPlaySessionRequest();
    }
}

bool FLevelPIEManager::LoadScene(const FString& InSceneReference)
{
    if (!EditorEngine || !IsPlayingInEditor() || InSceneReference.empty())
    {
        UE_LOG_CATEGORY(EditorEngine, Warning, "[SceneLoad] Ignored PIE load request. IsPlayingInEditor=%d Scene=%s",
                        IsPlayingInEditor() ? 1 : 0, InSceneReference.c_str());
        return false;
    }

    std::filesystem::path ChosenPath;
    const std::filesystem::path RawPath = FPaths::ToWide(InSceneReference);
    const std::filesystem::path SceneDir = FSceneSaveManager::GetSceneDirectory();

    auto TrySetChosenPath = [&ChosenPath](const std::filesystem::path& Candidate)
    {
        if (!Candidate.empty() && std::filesystem::exists(Candidate))
        {
            ChosenPath = Candidate;
            return true;
        }
        return false;
    };

    if (RawPath.is_absolute())
    {
        TrySetChosenPath(RawPath);
    }
    else
    {
        TrySetChosenPath(RawPath);
        if (ChosenPath.empty())
        {
            TrySetChosenPath(SceneDir / RawPath);
        }
    }

    if (ChosenPath.empty())
    {
        const bool bHasSceneExtension = EndsWithIgnoreCase(InSceneReference, ".scene");
        const bool bHasUmapExtension = EndsWithIgnoreCase(InSceneReference, ".umap");
        if (bHasSceneExtension || bHasUmapExtension)
        {
            const std::filesystem::path FileName = RawPath.filename();
            if (!TrySetChosenPath(SceneDir / FileName))
            {
                UE_LOG_CATEGORY(EditorEngine, Error, "[SceneLoad] Failed to resolve scene path from reference: %s",
                                InSceneReference.c_str());
                FNotificationManager::Get().AddNotification("Scene load failed: " + InSceneReference, ENotificationType::Error, 3.0f);
                return false;
            }
        }
        else
        {
            const std::wstring StemW = FPaths::ToWide(InSceneReference);
            if (!TrySetChosenPath(SceneDir / (StemW + L".umap")))
            {
                if (!TrySetChosenPath(SceneDir / (StemW + FSceneSaveManager::SceneExtension)))
                {
                    UE_LOG_CATEGORY(EditorEngine, Error, "[SceneLoad] Failed to find scene file for reference: %s",
                                    InSceneReference.c_str());
                    FNotificationManager::Get().AddNotification("Scene not found: " + InSceneReference, ENotificationType::Error, 3.0f);
                    return false;
                }
            }
        }
    }

    FWorldContext* Context = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
    if (!Context)
    {
        UE_LOG_CATEGORY(EditorEngine, Error, "[SceneLoad] No active world context for handle: %s",
                        EditorEngine->GetActiveWorldHandle().ToString().c_str());
        return false;
    }

    UE_LOG_CATEGORY(EditorEngine, Info, "[SceneLoad] Loading PIE scene '%s' from '%s'", InSceneReference.c_str(),
                    FPaths::ToUtf8(ChosenPath.wstring()).c_str());

    if (IRenderPipeline* Pipeline = EditorEngine->GetRenderPipeline())
    {
        Pipeline->OnSceneCleared();
    }

    EditorEngine->GetSelectionManager().ClearSelection();
    EditorEngine->GetSelectionManager().SetWorld(nullptr);

    if (UGameViewportClient* PIEViewportClient = EditorEngine->GetGameViewportClient())
    {
        PIEViewportClient->UnPossess();
    }

    if (Context->World)
    {
        Context->World->EndPlay();
        UObjectManager::Get().DestroyObject(Context->World);
        Context->World = nullptr;
    }

    FPerspectiveCameraData DummyCamera;
    const FString FilePath = FPaths::ToUtf8(ChosenPath.wstring());
    if (EndsWithIgnoreCase(FilePath, ".umap"))
    {
        Context->World = UObjectManager::Get().CreateObject<UWorld>();
        FSceneSaveManager::LoadWorldFromBinary(FilePath, Context->World);
        Context->WorldType = EWorldType::PIE;
        Context->ContextName = RawPath.stem().empty() ? "PIE" : FPaths::ToUtf8(RawPath.stem().wstring());
        Context->ContextHandle = EditorEngine->GetActiveWorldHandle();
    }
    else
    {
        FSceneSaveManager::LoadSceneFromJSON(FilePath, *Context, DummyCamera);
        Context->WorldType = EWorldType::PIE;
        Context->ContextHandle = EditorEngine->GetActiveWorldHandle();
    }

    EditorEngine->SetActiveWorld(Context->ContextHandle);

    if (!Context->World)
    {
        UE_LOG_CATEGORY(EditorEngine, Error, "[SceneLoad] Context world is null after loading '%s'", InSceneReference.c_str());
        FNotificationManager::Get().AddNotification("Scene load failed: " + InSceneReference, ENotificationType::Error, 3.0f);
        return false;
    }

    Context->World->SetWorldType(EWorldType::PIE);
    EditorEngine->GetSelectionManager().SetWorld(Context->World);
    Context->World->WarmupPickingData();
    EnsurePIEActiveCamera(Context->World, DummyCamera);
    if (!Context->World->HasBegunPlay())
    {
        Context->World->BeginPlay();
    }
    EnsurePIEActiveCamera(Context->World, DummyCamera);

    if (UGameViewportClient* PIEViewportClient = EditorEngine->GetGameViewportClient())
    {
        if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetViewportLayout().GetActiveViewport())
        {
            PIEViewportClient->SetViewport(ActiveVC->GetViewport());
            PIEViewportClient->SetCursorClipRect(ActiveVC->GetViewportScreenRect());
        }

        if (UCameraComponent* GameCamera = EnsurePIEActiveCamera(Context->World, DummyCamera))
        {
            PIEViewportClient->Possess(GameCamera);
        }
        PIEViewportClient->SetSpectatorCameraMovementEnabled(!HasGameplayCamera(Context->World));
    }

    FNotificationManager::Get().AddNotification("Loaded scene: " + InSceneReference, ENotificationType::Success, 2.0f);
    UE_LOG_CATEGORY(EditorEngine, Info, "[SceneLoad] Loaded PIE scene successfully: %s", InSceneReference.c_str());
    return true;
}

void FLevelPIEManager::RenderOverlayPopups()
{
    if (!EditorEngine || !IsPlayingInEditor())
    {
        return;
    }

    const FRect* AnchorRect = nullptr;
    if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetViewportLayout().GetActiveViewport())
    {
        AnchorRect = &ActiveVC->GetViewportScreenRect();
    }

    PIEOverlay.RenderWithinCurrentFrame(AnchorRect);
}

void FLevelPIEManager::RequestPlaySession(const FRequestPlaySessionParams& InParams)
{
    PlaySessionRequest = InParams;
}

void FLevelPIEManager::CancelRequestPlaySession()
{
    PlaySessionRequest.reset();
}

bool FLevelPIEManager::HasPlaySessionRequest() const
{
    return PlaySessionRequest.has_value();
}

void FLevelPIEManager::RequestEndPlayMap()
{
    if (!PlayInEditorSessionInfo.has_value())
    {
        return;
    }

    bRequestEndPlayMapQueued = true;
}

bool FLevelPIEManager::IsPlayingInEditor() const
{
    return PlayInEditorSessionInfo.has_value();
}

EPIEControlMode FLevelPIEManager::GetPIEControlMode() const
{
    return PIEControlMode;
}

bool FLevelPIEManager::IsPIEPossessedMode() const
{
    return IsPlayingInEditor() && PIEControlMode == EPIEControlMode::Possessed;
}

bool FLevelPIEManager::IsPIEEjectedMode() const
{
    return IsPlayingInEditor() && PIEControlMode == EPIEControlMode::Ejected;
}

bool FLevelPIEManager::TogglePIEControlMode()
{
    if (!IsPlayingInEditor())
    {
        return false;
    }

    if (PIEControlMode == EPIEControlMode::Possessed)
    {
        return EnterPIEEjectedMode();
    }

    return EnterPIEPossessedMode();
}

void FLevelPIEManager::StopPlayInEditorImmediate()
{
    if (IsPlayingInEditor())
    {
        EndPlayMap();
    }
}

void FLevelPIEManager::OpenScoreSavePopup(int32 InScore)
{
    PIEOverlay.OpenScoreSavePopup(InScore);
}

bool FLevelPIEManager::ConsumeScoreSavePopupResult(FString& OutNickname)
{
    return PIEOverlay.ConsumeScoreSavePopupResult(OutNickname);
}

void FLevelPIEManager::OpenMessagePopup(const FString& InMessage)
{
    PIEOverlay.OpenMessagePopup(InMessage);
}

bool FLevelPIEManager::ConsumeMessagePopupConfirmed()
{
    return PIEOverlay.ConsumeMessagePopupConfirmed();
}

void FLevelPIEManager::OpenScoreboardPopup(const FString& InFilePath)
{
    PIEOverlay.OpenScoreboardPopup(InFilePath);
}

void FLevelPIEManager::OpenTitleOptionsPopup()
{
    PIEOverlay.OpenTitleOptionsPopup();
}

void FLevelPIEManager::OpenTitleCreditsPopup()
{
    PIEOverlay.OpenTitleCreditsPopup();
}

bool FLevelPIEManager::IsScoreSavePopupOpen() const
{
    return PIEOverlay.IsScoreSavePopupOpen();
}

void FLevelPIEManager::StartQueuedPlaySessionRequest()
{
    if (!PlaySessionRequest.has_value())
    {
        return;
    }

    const FRequestPlaySessionParams Params = *PlaySessionRequest;
    PlaySessionRequest.reset();

    if (PlayInEditorSessionInfo.has_value())
    {
        EndPlayMap();
    }

    switch (Params.SessionDestination)
    {
    case EPIESessionDestination::InProcess:
        StartPlayInEditorSession(Params);
        break;
    }
}

void FLevelPIEManager::StartPlayInEditorSession(const FRequestPlaySessionParams& Params)
{
    if (!EditorEngine)
    {
        return;
    }

    EditorEngine->SetGamePaused(false);
    FInputManager::Get().ResetAllKeyStates();
    FAudioManager::Get().StopAll();

    UWorld* EditorWorld = EditorEngine->GetWorld();
    if (!EditorWorld)
    {
        return;
    }

    UWorld* PIEWorld = EditorWorld->DuplicateAs(EWorldType::PIE);
    if (!PIEWorld)
    {
        return;
    }

    FWorldContext Ctx;
    Ctx.WorldType = EWorldType::PIE;
    Ctx.ContextHandle = FName("PIE");
    Ctx.ContextName = "PIE";
    Ctx.World = PIEWorld;
    EditorEngine->GetWorldList().push_back(Ctx);

    FPlayInEditorSessionInfo Info;
    Info.OriginalRequestParams = Params;
    Info.PIEStartTime = 0.0;
    Info.PreviousActiveWorldHandle = EditorEngine->GetActiveWorldHandle();
    if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetViewportLayout().GetActiveViewport())
    {
        if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
        {
            Info.SavedViewportCamera.Location = VCCamera->GetWorldLocation();
            Info.SavedViewportCamera.Rotation = VCCamera->GetRelativeRotation();
            Info.SavedViewportCamera.CameraState = VCCamera->GetCameraState();
            Info.SavedViewportCamera.bValid = true;
        }
    }
    PlayInEditorSessionInfo = Info;

    EditorEngine->SetActiveWorld(FName("PIE"));

    if (IRenderPipeline* Pipeline = EditorEngine->GetRenderPipeline())
    {
        Pipeline->OnSceneCleared();
    }

    FPerspectiveCameraData FallbackCameraData;
    FallbackCameraData.Location = Info.SavedViewportCamera.Location;
    FallbackCameraData.Rotation = Info.SavedViewportCamera.Rotation.ToVector();
    FallbackCameraData.FOV = Info.SavedViewportCamera.CameraState.FOV;
    FallbackCameraData.NearClip = Info.SavedViewportCamera.CameraState.NearZ;
    FallbackCameraData.FarClip = Info.SavedViewportCamera.CameraState.FarZ;
    FallbackCameraData.bValid = Info.SavedViewportCamera.bValid;
    UCameraComponent* PIEFallbackCamera = EnsurePIEActiveCamera(PIEWorld, FallbackCameraData);

    EditorEngine->GetSelectionManager().ClearSelection();
    EditorEngine->GetSelectionManager().SetGizmoEnabled(false);
    EditorEngine->GetSelectionManager().SetWorld(PIEWorld);

    if (!EditorEngine->GetGameViewportClient())
    {
        UGameViewportClient* PIEViewportClient = UObjectManager::Get().CreateObject<UGameViewportClient>();
        EditorEngine->SetGameViewportClient(PIEViewportClient);
    }
    if (UGameViewportClient* PIEViewportClient = EditorEngine->GetGameViewportClient())
    {
        if (EditorEngine->GetWindow())
        {
            PIEViewportClient->SetOwnerWindow(EditorEngine->GetWindow()->GetHWND());
        }

        UCameraComponent* InitialTargetCamera = PIEWorld->GetActiveCamera();
        FViewport* InitialViewport = nullptr;
        if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetViewportLayout().GetActiveViewport())
        {
            InitialViewport = ActiveVC->GetViewport();
            PIEViewportClient->SetCursorClipRect(ActiveVC->GetViewportScreenRect());
        }
        PIEViewportClient->OnBeginPIE(InitialTargetCamera, InitialViewport);
    }
    EnterPIEPossessedMode();

    PIEWorld->BeginPlay();

    if (PIEWorld->GetActiveCamera() == PIEFallbackCamera)
    {
        if (UCameraComponent* SceneCamera = FindFirstCameraComponent(PIEWorld))
        {
            if (SceneCamera != PIEFallbackCamera)
            {
                PIEWorld->SetActiveCamera(SceneCamera);
            }
        }
    }

    if (UGameViewportClient* PIEViewportClient = EditorEngine->GetGameViewportClient())
    {
        if (UCameraComponent* GameCamera = PIEWorld->GetActiveCamera())
        {
            PIEViewportClient->Possess(GameCamera);
        }
        PIEViewportClient->SetSpectatorCameraMovementEnabled(!HasGameplayCamera(PIEWorld));
    }
}

void FLevelPIEManager::EndPlayMap()
{
    if (!EditorEngine)
    {
        return;
    }

    EditorEngine->SetGamePaused(false);
    FAudioManager::Get().StopAll();
    if (!PlayInEditorSessionInfo.has_value())
    {
        return;
    }

    const FName PrevHandle = PlayInEditorSessionInfo->PreviousActiveWorldHandle;
    EditorEngine->SetActiveWorld(PrevHandle);

    if (UWorld* EditorWorld = EditorEngine->GetWorld())
    {
        EditorWorld->GetScene().MarkAllPerObjectCBDirty();

        if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetViewportLayout().GetActiveViewport())
        {
            if (UCameraComponent* VCCamera = ActiveVC->GetCamera())
            {
                if (PlayInEditorSessionInfo->SavedViewportCamera.bValid)
                {
                    const FPIEViewportCameraSnapshot& SavedCamera = PlayInEditorSessionInfo->SavedViewportCamera;
                    VCCamera->SetWorldLocation(SavedCamera.Location);
                    VCCamera->SetRelativeRotation(SavedCamera.Rotation);
                    VCCamera->SetCameraState(SavedCamera.CameraState);
                }

                EditorWorld->SetActiveCamera(VCCamera);
            }
        }
    }

    EditorEngine->GetSelectionManager().ClearSelection();
    EditorEngine->GetSelectionManager().SetGizmoEnabled(true);
    EditorEngine->GetSelectionManager().SetWorld(EditorEngine->GetWorld());

    if (UGameViewportClient* PIEViewportClient = EditorEngine->GetGameViewportClient())
    {
        PIEViewportClient->OnEndPIE();
        UObjectManager::Get().DestroyObject(PIEViewportClient);
        EditorEngine->SetGameViewportClient(nullptr);
    }

    EditorEngine->DestroyWorldContext(FName("PIE"));

    if (IRenderPipeline* Pipeline = EditorEngine->GetRenderPipeline())
    {
        Pipeline->OnSceneCleared();
    }

    PlayInEditorSessionInfo.reset();
    PIEControlMode = EPIEControlMode::Possessed;
}

bool FLevelPIEManager::EnterPIEPossessedMode()
{
    if (!IsPlayingInEditor())
    {
        return false;
    }

    PIEControlMode = EPIEControlMode::Possessed;
    SyncGameViewportPIEControlState(true);
    FInputManager::Get().ResetAllKeyStates();
    return true;
}

bool FLevelPIEManager::EnterPIEEjectedMode()
{
    if (!IsPlayingInEditor())
    {
        return false;
    }

    PIEControlMode = EPIEControlMode::Ejected;
    SyncGameViewportPIEControlState(false);
    FInputManager::Get().ResetAllKeyStates();
    return true;
}

void FLevelPIEManager::SyncGameViewportPIEControlState(bool bPossessedMode)
{
    if (!EditorEngine)
    {
        return;
    }

    UGameViewportClient* PIEViewportClient = EditorEngine->GetGameViewportClient();
    if (!PIEViewportClient)
    {
        return;
    }

    PIEViewportClient->SetPIEPossessedInputEnabled(bPossessedMode);
    PIEViewportClient->SetSpectatorCameraMovementEnabled(false);
    if (!bPossessedMode)
    {
        return;
    }

    if (!IsScoreSavePopupOpen())
    {
        FInputManager::Get().SetGuiCaptureOverride(false, false, false);
    }

    if (EditorEngine->GetWindow())
    {
        PIEViewportClient->SetOwnerWindow(EditorEngine->GetWindow()->GetHWND());
    }

    if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetViewportLayout().GetActiveViewport())
    {
        PIEViewportClient->SetViewport(ActiveVC->GetViewport());
        PIEViewportClient->SetCursorClipRect(ActiveVC->GetViewportScreenRect());
    }

    if (UWorld* World = EditorEngine->GetWorld())
    {
        PIEViewportClient->SetSpectatorCameraMovementEnabled(!HasGameplayCamera(World));
        if (UCameraComponent* GameCamera = World->GetActiveCamera())
        {
            PIEViewportClient->Possess(GameCamera);
            return;
        }
    }

    if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetViewportLayout().GetActiveViewport())
    {
        PIEViewportClient->Possess(ActiveVC->GetCamera());
    }
}
