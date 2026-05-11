#include "LevelEditor/Scene/LevelSceneManager.h"

#include "Common/File/EditorFileUtils.h"
#include "EditorEngine.h"
#include "Common/Viewport/EditorViewportCamera.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Object/Object.h"
#include "LevelEditor/Settings/LevelEditorSettings.h"

void FLevelSceneManager::Init(UEditorEngine* InEditorEngine)
{
    EditorEngine = InEditorEngine;
}

void FLevelSceneManager::Shutdown()
{
    bRequestSaveSceneAsDialogQueued = false;
    CurrentLevelFilePath.clear();
    EditorEngine = nullptr;
}

void FLevelSceneManager::Tick(float DeltaTime)
{
    (void)DeltaTime;
    ProcessDeferredActions();
}

void FLevelSceneManager::CloseScene()
{
    ClearScene();
}

void FLevelSceneManager::NewScene()
{
    if (!EditorEngine)
    {
        return;
    }

    EditorEngine->StopPlayInEditorImmediate();
    ClearScene();

    FWorldContext& Ctx = EditorEngine->CreateWorldContext(EWorldType::Editor, FName("NewScene"), "New Scene");
    Ctx.World->InitWorld();
    EditorEngine->SetActiveWorld(Ctx.ContextHandle);
    EditorEngine->GetSelectionManager().SetWorld(EditorEngine->GetWorld());
    EditorEngine->ResetViewport();

    CurrentLevelFilePath.clear();
}

void FLevelSceneManager::LoadStartLevel()
{
    if (!EditorEngine)
    {
        return;
    }

    const FString& StartLevel = FLevelEditorSettings::Get().EditorStartLevel;
    if (StartLevel.empty())
    {
        return;
    }

    const std::filesystem::path ScenePath =
        std::filesystem::path(FSceneSaveManager::GetSceneDirectory()) / (FPaths::ToWide(StartLevel) + FSceneSaveManager::SceneExtension);
    const FString FilePath = FPaths::ToUtf8(ScenePath.wstring());

    if (!LoadSceneFromPath(FilePath))
    {
        NewScene();
    }
}

void FLevelSceneManager::ClearScene()
{
    if (!EditorEngine)
    {
        return;
    }

    EditorEngine->StopPlayInEditorImmediate();
    DestroyCurrentSceneWorlds(true, true);
}

bool FLevelSceneManager::LoadSceneFromPath(const FString& InScenePath)
{
    if (!EditorEngine || InScenePath.empty())
    {
        return false;
    }

    EditorEngine->StopPlayInEditorImmediate();
    ClearScene();

    FWorldContext LoadContext;
    FPerspectiveCameraData CameraData;

    if (FSceneSaveManager::IsJsonFile(InScenePath))
    {
        FSceneSaveManager::LoadSceneFromJSON(InScenePath, LoadContext, CameraData);
    }
    else if (InScenePath.ends_with(".umap") || InScenePath.ends_with(".UMAP"))
    {
        LoadContext.World = UObjectManager::Get().CreateObject<UWorld>();
        FSceneSaveManager::LoadWorldFromBinary(InScenePath, LoadContext.World);
        LoadContext.WorldType = EWorldType::Editor;
        LoadContext.ContextName = "Loaded Binary Scene";
        LoadContext.ContextHandle = FName("Loaded Binary Scene");
    }

    if (!LoadContext.World)
    {
        return false;
    }

    EditorEngine->GetWorldList().push_back(LoadContext);
    EditorEngine->SetActiveWorld(LoadContext.ContextHandle);
    EditorEngine->GetSelectionManager().SetWorld(LoadContext.World);
    LoadContext.World->WarmupPickingData();
    EditorEngine->ResetViewport();
    EditorEngine->RestoreViewportCamera(CameraData);

    CurrentLevelFilePath = InScenePath;
    return true;
}

bool FLevelSceneManager::LoadSceneWithDialog()
{
    if (!EditorEngine)
    {
        return false;
    }

    const std::wstring InitialDir = FSceneSaveManager::GetSceneDirectory();
    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"Scene Files (*.Scene;*.umap)\0*.Scene;*.umap\0All Files (*.*)\0*.*\0",
        .Title = L"Load Scene",
        .InitialDirectory = InitialDir.c_str(),
        .OwnerWindowHandle = EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr,
        .bFileMustExist = true,
        .bPathMustExist = true,
        .bPromptOverwrite = false,
        .bReturnRelativeToProjectRoot = false,
    });
    if (SelectedPath.empty())
    {
        return false;
    }

    return LoadSceneFromPath(SelectedPath);
}

bool FLevelSceneManager::SaveScene()
{
    if (HasCurrentLevelFilePath())
    {
        return SaveSceneAs(CurrentLevelFilePath);
    }

    return SaveSceneAsWithDialog();
}

bool FLevelSceneManager::SaveSceneAs(const FString& InScenePath)
{
    if (!EditorEngine || InScenePath.empty())
    {
        return false;
    }

    EditorEngine->StopPlayInEditorImmediate();
    FWorldContext* Context = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
    if (!Context || !Context->World)
    {
        return false;
    }

    if (InScenePath.ends_with(".umap") || InScenePath.ends_with(".UMAP"))
    {
        FSceneSaveManager::SaveWorldToBinary(InScenePath, Context->World);
    }
    else
    {
        FEditorViewportCamera* SaveCamera = EditorEngine->FindSceneViewportCamera();
        FSceneSaveManager::SaveSceneAsJSON(InScenePath, *Context, SaveCamera ? &SaveCamera->GetCameraState() : nullptr);
    }

    CurrentLevelFilePath = InScenePath;
    return true;
}

void FLevelSceneManager::RequestSaveSceneAsDialog()
{
    bRequestSaveSceneAsDialogQueued = true;
}

bool FLevelSceneManager::SaveSceneAsWithDialog()
{
    if (!EditorEngine)
    {
        return false;
    }

    const std::wstring InitialDir = FSceneSaveManager::GetSceneDirectory();
    const std::wstring DefaultFile =
        HasCurrentLevelFilePath() ? std::filesystem::path(FPaths::ToWide(CurrentLevelFilePath)).filename().wstring() : std::wstring(L"Untitled");
    const FString SelectedPath = FEditorFileUtils::SaveFileDialog({
        .Filter = L"Binary Scene (*.umap)\0*.umap\0JSON Scene (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0",
        .Title = L"Save Scene As",
        .InitialDirectory = InitialDir.c_str(),
        .DefaultFileName = DefaultFile.c_str(),
        .OwnerWindowHandle = EditorEngine->GetWindow() ? EditorEngine->GetWindow()->GetHWND() : nullptr,
        .bFileMustExist = false,
        .bPathMustExist = true,
        .bPromptOverwrite = true,
        .bReturnRelativeToProjectRoot = false,
    });
    if (SelectedPath.empty())
    {
        return false;
    }

    return SaveSceneAs(SelectedPath);
}

void FLevelSceneManager::ProcessDeferredActions()
{
    if (!bRequestSaveSceneAsDialogQueued)
    {
        return;
    }

    bRequestSaveSceneAsDialogQueued = false;
    SaveSceneAsWithDialog();
}

void FLevelSceneManager::DestroyCurrentSceneWorlds(bool bClearHistory, bool bResetLevelPath)
{
    if (!EditorEngine)
    {
        return;
    }

    if (bClearHistory)
    {
        EditorEngine->ClearTrackedTransformHistory();
    }

    EditorEngine->GetSelectionManager().ClearSelection();
    EditorEngine->GetSelectionManager().SetWorld(nullptr);

    EditorEngine->InvalidateOcclusionResults();

    for (FWorldContext& Ctx : EditorEngine->GetWorldList())
    {
        Ctx.World->EndPlay();
        UObjectManager::Get().DestroyObject(Ctx.World);
    }

    EditorEngine->GetWorldList().clear();
    EditorEngine->SetActiveWorld(FName::None);
    EditorEngine->InvalidateTrackedSceneSnapshotCache();

    if (bResetLevelPath)
    {
        CurrentLevelFilePath.clear();
    }

    EditorEngine->GetViewportLayout().DestroyAllCameras();
}
