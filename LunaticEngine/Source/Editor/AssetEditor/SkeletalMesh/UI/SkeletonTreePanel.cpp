#include "AssetEditor/SkeletalMesh/UI/SkeletonTreePanel.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

#include <cstdio>

void FSkeletonTreePanel::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State)
{
    ImGui::BeginChild("##SkeletonTreePanel", ImVec2(0.0f, 0.0f), true);

    ImGui::TextUnformatted("Skeleton Tree");
    ImGui::Separator();

    if (!Mesh)
    {
        ImGui::TextDisabled("No Skeleton.");
        ImGui::EndChild();
        return;
    }

    // 김형도 담당 영역 Placeholder:
    // 현재 Viewer 단계에서는 Bone hierarchy API가 확정되지 않아 flat bone list로 표시한다.
    // 추후 USkeleton / FBoneInfo에서 BoneName, ParentIndex, Local/World Transform accessor가 확정되면
    // 이 영역을 재귀 TreeNode + Transform Inspector + Bone Gizmo 선택 상태와 연결하면 된다.
    const int32 BoneCount = Mesh->GetBoneCount();
    if (BoneCount <= 0)
    {
        ImGui::TextDisabled("This mesh has no bone data.");
        ImGui::EndChild();
        return;
    }

    ImGui::TextDisabled("Hierarchy placeholder - flat bone list until USkeleton accessor is ready.");
    ImGui::Separator();

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        char Label[64];
        snprintf(Label, sizeof(Label), "Bone %d", BoneIndex);

        const bool bSelected = (State.SelectedBoneIndex == BoneIndex);
        if (ImGui::Selectable(Label, bSelected))
        {
            State.SelectedBoneIndex = BoneIndex;
        }
    }

    ImGui::EndChild();
}
