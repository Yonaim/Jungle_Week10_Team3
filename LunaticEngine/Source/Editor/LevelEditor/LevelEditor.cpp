#include "LevelEditor/LevelEditor.h"
#include "EditorEngine.h"


void FLevelEditor::Init(UEditorEngine *InEditorEngine, FWindowsWindow *InWindow, FRenderer &InRenderer)
{
    EditorEngine = InEditorEngine;

    SelectionManager.Init();
    SelectionManager.SetWorld(EditorEngine->GetWorld());

    ViewportLayout.Init(EditorEngine, InWindow, InRenderer, &SelectionManager);

    ViewportLayout.LoadFromSettings();
    PIEManager.Init(EditorEngine);
    HistoryManager.Init(EditorEngine);
    SceneManager.Init(EditorEngine);
}

void FLevelEditor::Shutdown()
{
    SceneManager.Shutdown();
    HistoryManager.Shutdown();
    PIEManager.Shutdown();
    SelectionManager.Shutdown();
    ViewportLayout.Release();
    EditorEngine = nullptr;
}
