#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"

class USkeletalMesh;

class FSkeletalMeshEditorToolbar
{
  public:
    void Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State);
};
