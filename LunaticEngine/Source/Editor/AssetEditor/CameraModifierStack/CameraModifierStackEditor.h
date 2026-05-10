#pragma once

#include "AssetEditor/IAssetEditor.h"
#include "Core/CoreTypes.h"

#include <filesystem>
#include <functional>

class UAssetData;
class UObject;
class UCameraModifierStackAssetData;
class UEditorEngine;
class FRenderer;
struct FAssetBezierCurve;
struct FCameraShakeModifierAssetDesc;

class FCameraModifierStackEditor final : public IAssetEditor
{
  public:
    void Init(UEditorEngine *InEditorEngine, FRenderer *InRenderer) override;
    void Shutdown() override;

    bool CanEdit(UObject *Asset) const override;
    bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) override;
    void Close() override;
    bool Save() override;

    void Render(float DeltaTime) override;

    bool IsOpen() const override { return bOpen; }
    bool IsDirty() const override { return bDirty; }
    bool IsCapturingInput() const override { return bCapturingInput; }
    const char *GetEditorName() const override { return "CameraModifierStackEditor"; }

    bool CreateCameraShakeAsset();
    bool HasOpenAsset() const { return EditingAsset != nullptr; }

  private:
    void DrawToolbar();
    void DrawAssetContents();
    void DrawDetails();
    bool DrawLabeledField(const char *Label, const std::function<bool()> &DrawField);
    bool DrawCurveControlPointRow(const char *Label, float &XValue, float &YValue);
    bool DrawCurveEditor(const char *Label, FAssetBezierCurve &Curve);
    bool PromptForSavePath(void *OwnerWindowHandle = nullptr);
    void DrawCameraModifierStackAssetContents(UCameraModifierStackAssetData &Asset);
    void DrawCameraModifierStackAssetDetails(UCameraModifierStackAssetData &Asset);
    bool DrawCameraShakeDetails(FCameraShakeModifierAssetDesc &Desc);
    void MarkDirty() { bDirty = true; }

    FCameraShakeModifierAssetDesc *FindSelectedCameraShake(UCameraModifierStackAssetData &Asset);

  private:
    UEditorEngine        *EditorEngine = nullptr;
    FRenderer            *Renderer = nullptr;
    UAssetData           *EditingAsset = nullptr;
    std::filesystem::path EditingAssetPath;
    uint64                SelectedEditorId = 0;
    bool                  bOpen = false;
    bool                  bDirty = false;
    bool                  bCapturingInput = false;
};
