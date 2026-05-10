#include "Scene/EditorSceneManager.h"

#include "Common/File/EditorFileUtils.h"
#include "EditorEngine.h"
#include "Engine/Platform/Paths.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Serialization/SceneSaveManager.h"
#include "Object/Object.h"
#include "Settings/EditorSettings.h"

void FEditorSceneManager::Init(UEditorEngine* InEditorEngine)
{
    EditorEngine = InEditorEngine;
}

void FEditorSceneManager::Shutdown()
{
    bRequestSaveSceneAsDialogQueued = false;
    CurrentLevelFilePath.clear();
    EditorEngine = nullptr;
}

void FEditorSceneManager::Tick(float DeltaTime)
{
    (void)DeltaTime;
    ProcessDeferredActions();
}

void FEditorSceneManager::CloseScene()
{
    ClearScene();
}

void FEditorSceneManager::NewScene()
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

void FEditorSceneManager::LoadStartLevel()
{
    if (!EditorEngine)
    {
        return;
    }

    const FString& StartLevel = FEditorSettings::Get().EditorStartLevel;
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

void FEditorSceneManager::ClearScene()
{
    if (!EditorEngine)
    {
        return;
    }

    EditorEngine->StopPlayInEditorImmediate();
    DestroyCurrentSceneWorlds(true, true);
}

bool FEditorSceneManager::LoadSceneFromPath(const FString& InScenePath)
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

bool FEditorSceneManager::LoadSceneWithDialog()
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

bool FEditorSceneManager::SaveScene()
{
    if (HasCurrentLevelFilePath())
    {
        return SaveSceneAs(CurrentLevelFilePath);
    }

    return SaveSceneAsWithDialog();
}

bool FEditorSceneManager::SaveSceneAs(const FString& InScenePath)
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
        FSceneSaveManager::SaveSceneAsJSON(InScenePath, *Context, EditorEngine->FindSceneViewportCamera());
    }

    CurrentLevelFilePath = InScenePath;
    return true;
}

void FEditorSceneManager::RequestSaveSceneAsDialog()
{
    bRequestSaveSceneAsDialogQueued = true;
}

bool FEditorSceneManager::SaveSceneAsWithDialog()
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

void FEditorSceneManager::ProcessDeferredActions()
{
    if (!bRequestSaveSceneAsDialogQueued)
    {
        return;
    }

    bRequestSaveSceneAsDialogQueued = false;
    SaveSceneAsWithDialog();
}

void FEditorSceneManager::DestroyCurrentSceneWorlds(bool bClearHistory, bool bResetLevelPath)
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
