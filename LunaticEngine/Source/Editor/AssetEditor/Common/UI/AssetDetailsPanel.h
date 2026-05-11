#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"
#include "Common/UI/Panels/Panel.h"

#include <filesystem>

class USkeletalMesh;

// 에셋 에디터 전용 Details 패널.
// 레벨 에디터 Details와 같은 공통 스타일/행 형식을 사용해 에셋 타입별 정보를 표시한다.
// Skeletal Mesh Editor는 이 패널을 사용하고, 이후 다른 에셋 에디터도 같은 패널 형식을 재사용한다.
class FAssetDetailsPanel
{
  public:
    void RenderSkeletalMesh(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath,
                            FSkeletalMeshEditorState &State, const FPanelDesc &PanelDesc);

  private:
    void RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State);
    void RenderLODSectionMaterialInfo(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State);
    void RenderViewerActions(FSkeletalMeshEditorState &State);
    void RenderRuntimePlaceholder();
    void RenderBoneEditingPlaceholder(FSkeletalMeshEditorState &State);
};
