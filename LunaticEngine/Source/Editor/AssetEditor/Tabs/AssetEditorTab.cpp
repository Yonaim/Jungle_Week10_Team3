#include "PCH/LunaticPCH.h"
#include "AssetEditor/Tabs/AssetEditorTab.h"

#include "AssetEditor/IAssetEditor.h"
#include "Platform/Paths.h"

#include <cctype>

namespace
{
    std::string SanitizeDockLayoutId(std::string Value)
    {
        if (Value.empty())
        {
            return "UntitledAsset";
        }

        for (char &Ch : Value)
        {
            const unsigned char C = static_cast<unsigned char>(Ch);
            if (!std::isalnum(C))
            {
                Ch = '_';
            }
        }
        return Value;
    }
}

FAssetEditorTab::FAssetEditorTab(std::unique_ptr<IAssetEditor> InEditor) : Editor(std::move(InEditor))
{
    std::string SourceId = "Asset";
    if (Editor)
    {
        const std::filesystem::path &AssetPath = Editor->GetAssetPath();
        SourceId = AssetPath.empty() ? Editor->GetEditorName() : FPaths::ToUtf8(AssetPath.generic_wstring());
    }
    LayoutId = std::string("AssetEditorDockSpace_") + SanitizeDockLayoutId(SourceId);
}

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
