#include "AssetEditor/Tabs/AssetEditorTab.h"

#include "AssetEditor/IAssetEditor.h"

FAssetEditorTab::FAssetEditorTab(std::unique_ptr<IAssetEditor> InEditor) : Editor(std::move(InEditor)) {}

IAssetEditor *FAssetEditorTab::GetEditor() const { return Editor.get(); }

const char *FAssetEditorTab::GetTitle() const
{
    return Editor ? Editor->GetEditorName() : "Asset";
}

const std::filesystem::path &FAssetEditorTab::GetAssetPath() const
{
    static const std::filesystem::path EmptyPath;
    return Editor ? Editor->GetAssetPath() : EmptyPath;
}

void FAssetEditorTab::Tick(float DeltaTime)
{
    if (Editor)
    {
        Editor->Tick(DeltaTime);
    }
}

void FAssetEditorTab::Render(float DeltaTime)
{
    if (Editor)
    {
        Editor->RenderContent(DeltaTime);
    }
}
