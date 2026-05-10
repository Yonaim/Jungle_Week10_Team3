#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"

#include <filesystem>

class USkeletalMesh;

class FSkeletalMeshDetailsPanel
{
  public:
    void Render(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State);

  private:
    void RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State);
    void RenderRuntimePlaceholder();
    void RenderBoneEditingPlaceholder(FSkeletalMeshEditorState &State);
};
