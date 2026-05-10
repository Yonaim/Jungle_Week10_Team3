#pragma once

#include "AssetEditor/Window/AssetEditorWindow.h"
#include "ImGui/imgui.h"

#include <filesystem>
#include <memory>

class UObject;
class UEditorEngine;
class FRenderer;
class IAssetEditor;
class USkeletalMesh;

/**
 * Asset Editor의 진입점이자 라우터.
 *
 * 역할:
 * - Content Browser / 메뉴에서 들어온 파일 열기 요청을 받는다.
 * - .uasset / .fbx 같은 입력 경로를 판별한다.
 * - UObject 타입에 맞는 IAssetEditor 인스턴스를 생성한다.
 * - 생성된 에디터를 AssetEditorWindow(현재는 패널 컨트롤러)에 등록한다.
 *
 * 주의:
 * - SkeletalMeshEditor는 FBX 파일을 직접 파싱하지 않는다.
 * - .fbx는 여기서 Preview용 USkeletalMesh로 변환한 뒤 SkeletalMeshEditor에 넘긴다.
 * - 실제 FBX SDK 기반 import/bake가 완성되면 OpenFbxForPreview() 내부의 Dummy 생성 경로를 교체한다.
 */
class FAssetEditorManager
{
  public:
    void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer);
    void Shutdown();

    void Tick(float DeltaTime);
    void RenderContent(float DeltaTime, ImGuiID DockspaceId = 0);

    bool OpenAssetFromPath(const std::filesystem::path &AssetPath);
    bool OpenSourceFileFromPath(const std::filesystem::path &SourcePath);
    bool OpenLoadedAsset(UObject *Asset, const std::filesystem::path &AssetPath);
    bool OpenAssetWithDialog(void *OwnerWindowHandle = nullptr);
    bool OpenFbxWithDialog(void *OwnerWindowHandle = nullptr);

    /**
     * FBX를 SkeletalMeshEditor에서 미리보기 위한 진입점.
     * 현재는 Dummy USkeletalMesh를 만들어 Viewer UI를 테스트한다.
     * 성원희 담당 FBX Importer가 준비되면 여기에서 실제 USkeletalMesh 생성 경로로 교체한다.
     */
    bool OpenFbxForPreview(const std::filesystem::path &FbxPath);

    /**
     * 빈 Asset Editor 패널을 표시한다.
     * 이 함수는 더 이상 Camera Modifier Stack을 자동 생성하지 않는다.
     */
    bool ShowAssetEditorWindow();
    bool CreateCameraModifierStackAsset();

    bool SaveActiveEditor();
    void CloseActiveEditor();

    bool IsCapturingInput() const;

    FAssetEditorWindow &GetAssetEditorWindow() { return AssetEditorWindow; }
    const FAssetEditorWindow &GetAssetEditorWindow() const { return AssetEditorWindow; }

  private:
    std::unique_ptr<IAssetEditor> CreateEditorForAsset(UObject *Asset) const;

    /**
     * 임시 Preview Mesh 생성 경로.
     * 실제 Importer가 완성되기 전까지 SkeletalMeshEditor 작업이 막히지 않도록 둔다.
     */
    USkeletalMesh *CreateDummySkeletalMeshForEditorPreview(const std::filesystem::path &SourcePath) const;

  private:
    UEditorEngine *EditorEngine = nullptr;
    FRenderer     *Renderer = nullptr;

    // 이름은 Window지만 현재 구현은 별도 창이 아니라 Level Editor DockSpace 안의 패널 컨트롤러다.
    FAssetEditorWindow AssetEditorWindow;
};
