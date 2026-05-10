#pragma once

#include <filesystem>

class UObject;
class UEditorEngine;
class FRenderer;

class IAssetEditor
{
  public:
    virtual ~IAssetEditor() = default;

    virtual void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer) = 0;

    virtual bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) = 0;
    virtual void Close() = 0;
    virtual bool Save() = 0;

    virtual void Tick(float DeltaTime) {}
    virtual void RenderContent(float DeltaTime) = 0;

    virtual bool IsDirty() const = 0;
    virtual bool IsCapturingInput() const = 0;

    virtual const char *GetEditorName() const = 0;
    virtual const std::filesystem::path &GetAssetPath() const = 0;
};
