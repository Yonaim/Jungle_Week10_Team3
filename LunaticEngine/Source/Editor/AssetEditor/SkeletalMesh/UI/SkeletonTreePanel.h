#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"

class USkeletalMesh;

class FSkeletonTreePanel
{
  public:
    void Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State);
};
