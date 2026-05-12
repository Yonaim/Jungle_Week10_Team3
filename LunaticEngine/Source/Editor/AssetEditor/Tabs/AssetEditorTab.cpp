#include "PCH/LunaticPCH.h"
#include "AssetEditor/Tabs/AssetEditorTab.h"

#include "AssetEditor/IAssetEditor.h"
#include "Platform/Paths.h"

FAssetEditorTab::FAssetEditorTab(std::unique_ptr<IAssetEditor> InEditor) : Editor(std::move(InEditor)) {}

IAssetEditor *FAssetEditorTab::GetEditor() const { return Editor.get(); }

std::string FAssetEditorTab::GetTitle() const
{
    if (!Editor)
    {
        return "Asset";
    }

    const std::filesystem::path &AssetPath = Editor->GetAssetPath();
    if (!AssetPath.empty())
    {
        return FPaths::ToUtf8(AssetPath.filename().wstring());
    }

    return Editor->GetEditorName();
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
