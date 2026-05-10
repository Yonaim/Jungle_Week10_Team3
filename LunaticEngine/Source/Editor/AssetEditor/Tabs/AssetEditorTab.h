#pragma once

#include "AssetEditor/IAssetEditor.h"

#include <filesystem>
#include <memory>

class FAssetEditorTab
{
  public:
    explicit FAssetEditorTab(std::unique_ptr<IAssetEditor> InEditor);

    IAssetEditor *GetEditor() const;
    const char *GetTitle() const;
    const std::filesystem::path &GetAssetPath() const;

    void Tick(float DeltaTime);
    void Render(float DeltaTime);

  private:
    std::unique_ptr<IAssetEditor> Editor;
};
