#include "AssetEditor/SkeletalMesh/UI/SkeletonTreePanel.h"

#include "Common/UI/EditorPanel.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

#include <cctype>
#include <cstdio>
#include <string>

namespace
{
bool ContainsCaseInsensitive(const std::string &Text, const char *Pattern)
{
    if (!Pattern || Pattern[0] == '\0')
    {
        return true;
    }

    std::string LowerText = Text;
    std::string LowerPattern = Pattern;
    for (char &Ch : LowerText)
    {
        Ch = static_cast<char>(::tolower(static_cast<unsigned char>(Ch)));
    }
    for (char &Ch : LowerPattern)
    {
        Ch = static_cast<char>(::tolower(static_cast<unsigned char>(Ch)));
    }
    return LowerText.find(LowerPattern) != std::string::npos;
}
}

void FSkeletonTreePanel::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, const FEditorPanelDesc &PanelDesc)
{
    if (!FEditorPanel::Begin(PanelDesc))
    {
        FEditorPanel::End();
        return;
    }

    if (!Mesh)
    {
        ImGui::TextDisabled("No Skeleton.");
        FEditorPanel::End();
        return;
    }

    const int32 BoneCount = Mesh->GetBoneCount();
    ImGui::Text("Bones: %d", BoneCount);
    ImGui::SameLine();
    if (ImGui::Button("Clear Selection"))
    {
        State.SelectedBoneIndex = -1;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##SkeletalBoneSearch", "Search bone...", SearchBuffer, sizeof(SearchBuffer));

    ImGui::Separator();

    // 김형도 담당 영역 Placeholder:
    // 현재 Viewer 단계에서는 Bone hierarchy API가 확정되지 않아 flat bone list로 표시한다.
    // 추후 USkeleton / FBoneInfo에서 BoneName, ParentIndex, Local/World Transform accessor가 확정되면
    // 이 영역을 재귀 TreeNode + Transform Inspector + Bone Gizmo 선택 상태와 연결하면 된다.
    if (BoneCount <= 0)
    {
        ImGui::TextDisabled("This mesh has no bone data.");
        ImGui::TextDisabled("FBX Importer가 skeleton/bone 데이터를 채우면 여기에 표시된다.");
        FEditorPanel::End();
        return;
    }

    ImGui::TextDisabled("Hierarchy placeholder - flat bone list until USkeleton accessor is ready.");

    if (ImGui::BeginChild("##SkeletalBoneList", ImVec2(0, 0), true))
    {
        for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
        {
            char Label[64];
            snprintf(Label, sizeof(Label), "Bone %d", BoneIndex);

            if (!ContainsCaseInsensitive(Label, SearchBuffer))
            {
                continue;
            }

            const bool bSelected = (State.SelectedBoneIndex == BoneIndex);
            if (ImGui::Selectable(Label, bSelected))
            {
                State.SelectedBoneIndex = BoneIndex;
            }
        }
    }
    ImGui::EndChild();

    FEditorPanel::End();
}
