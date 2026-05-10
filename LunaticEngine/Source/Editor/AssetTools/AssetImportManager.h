#pragma once

#include "Core/CoreTypes.h"

class UEditorEngine;

class FAssetImportManager
{
public:
    void Init(UEditorEngine* InEditorEngine);
    void Shutdown();

    bool ImportMaterialWithDialog();
    bool ImportTextureWithDialog();

private:
    bool ImportAssetFile(const FString& SourcePath, const std::wstring& RelativeDestinationDir, FString& OutImportedRelativePath);

private:
    UEditorEngine* EditorEngine = nullptr;
};
