#pragma once

#include "AssetEditor/SkeletalMesh/SkeletalMeshEditorTypes.h"

class USkeletalMesh;

class FSkeletalMeshPreviewViewport
{
  public:
    void Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, float DeltaTime);

  private:
    void RenderPlaceholderCanvas(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State);
};
