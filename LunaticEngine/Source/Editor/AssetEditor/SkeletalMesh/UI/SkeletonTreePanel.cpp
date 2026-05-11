#include "AssetEditor/SkeletalMesh/UI/SkeletonTreePanel.h"

#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"
#include "Common/UI/Panels/Panel.h"
#include "Common/UI/Style/EditorUIStyle.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace
{
bool ContainsCaseInsensitive(const std::string& Text, const char* Pattern)
{
    if (!Pattern || Pattern[0] == '\0')
    {
        return true;
    }

    std::string LowerText = Text;
    std::string LowerPattern = Pattern;
    for (char& Ch : LowerText)
    {
        Ch = static_cast<char>(::tolower(static_cast<unsigned char>(Ch)));
    }
    for (char& Ch : LowerPattern)
    {
        Ch = static_cast<char>(::tolower(static_cast<unsigned char>(Ch)));
    }
    return LowerText.find(LowerPattern) != std::string::npos;
}


void DrawBoneIconForLastTreeItem(int32 Depth, float CellStartScreenX, bool bSelected)
{
    ID3D11ShaderResourceView* BoneIcon = FEditorUIStyle::GetIcon("Editor.Icon.Bone");
    if (!BoneIcon)
    {
        return;
    }

    const ImVec2 RowMin = ImGui::GetItemRectMin();
    const ImVec2 RowMax = ImGui::GetItemRectMax();
    constexpr float IconSize = 14.0f;
    const float IconX = CellStartScreenX + FEditorUIStyle::SkeletonTreeIndentWidth * static_cast<float>(Depth) + 18.0f;
    const float IconY = RowMin.y + ((RowMax.y - RowMin.y) - IconSize) * 0.5f;
    const ImU32 Tint = bSelected ? IM_COL32(0, 0, 0, 255) : IM_COL32(210, 210, 210, 255);

    ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(BoneIcon),
                                         ImVec2(IconX, IconY),
                                         ImVec2(IconX + IconSize, IconY + IconSize),
                                         ImVec2(0.0f, 0.0f),
                                         ImVec2(1.0f, 1.0f),
                                         Tint);
}

void CollectTreeVisibleBoneOrder(const TArray<TArray<int32>>& BoneChildren, int32 BoneIndex, TArray<int32>& OutOrder)
{
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(BoneChildren.size()))
    {
        return;
    }

    OutOrder.push_back(BoneIndex);
    for (int32 ChildBoneIndex : BoneChildren[BoneIndex])
    {
        CollectTreeVisibleBoneOrder(BoneChildren, ChildBoneIndex, OutOrder);
    }
}
} // namespace

void FSkeletonTreePanel::Render(USkeletalMesh* Mesh, FSkeletalMeshEditorState& State,
                                FSkeletalMeshSelectionManager& SelectionManager, const FPanelDesc& PanelDesc)
{
    if (!FPanel::Begin(PanelDesc))
    {
        FPanel::End();
        return;
    }

    if (!Mesh)
    {
        ImGui::TextDisabled("No Skeleton.");
        FPanel::End();
        return;
    }

    const int32 BoneCount = Mesh->GetBoneCount();

    FEditorUIStyle::PushHeaderButtonStyle();
    if (ImGui::Button("Clear Selection"))
    {
        SelectionManager.ClearSelection();
        State.SelectedBoneIndex = -1;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Bones: %d / Selected: %d", BoneCount, SelectionManager.GetSelectedCount());
    FEditorUIStyle::PopHeaderButtonStyle();

    const float SearchWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x);
    FEditorUIStyle::DrawSearchInputWithIcon("##SkeletalBoneSearch", "Search bone...", SearchBuffer, sizeof(SearchBuffer),
                                            SearchWidth);

    ImGui::Separator();

    if (BoneCount <= 0)
    {
        ImGui::TextDisabled("This mesh has no bone data.");
        ImGui::TextDisabled("FBX Importer가 skeleton/bone 데이터를 채우면 여기에 표시된다.");
        FPanel::End();
        return;
    }

    const TArray<FBoneInfo>& Bones = Mesh->GetBones();

    if (Bones.empty())
    {
        ImGui::TextDisabled("Bone count is valid, but bone array is empty.");
        FPanel::End();
        return;
    }

    TArray<int32> VisibleBoneOrder;
    const bool bHasSearch = SearchBuffer[0] != '\0';
    if (bHasSearch)
    {
        for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Bones.size()); ++BoneIndex)
        {
            const FBoneInfo& Bone = Bones[BoneIndex];
            const char* BoneName = Bone.Name.empty() ? "(unnamed bone)" : Bone.Name.c_str();
            if (ContainsCaseInsensitive(BoneName, SearchBuffer))
            {
                VisibleBoneOrder.push_back(BoneIndex);
            }
        }
    }
    else
    {
        const TArray<TArray<int32>>& BoneChildren = Mesh->GetBoneChildren();
        const TArray<int32>& RootBoneIndices = Mesh->GetRootBoneIndices();
        if (BoneChildren.size() == Bones.size())
        {
            for (int32 RootBoneIndex : RootBoneIndices)
            {
                CollectTreeVisibleBoneOrder(BoneChildren, RootBoneIndex, VisibleBoneOrder);
            }
        }
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false))
    {
        SelectionManager.SelectAll(VisibleBoneOrder);
        SyncLegacySelectedBoneIndex(State, SelectionManager);
    }

    if (!FEditorUIStyle::BeginSkeletonTreeTable("##SkeletalBoneTreeTable"))
    {
        FPanel::End();
        return;
    }

    FEditorUIStyle::SetupSkeletonTreeTableColumns();

    if (bHasSearch)
    {
        DrawFilteredBoneList(Bones, State, SelectionManager, VisibleBoneOrder);
    }
    else
    {
        const TArray<TArray<int32>>& BoneChildren = Mesh->GetBoneChildren();
        const TArray<int32>& RootBoneIndices = Mesh->GetRootBoneIndices();

        if (RootBoneIndices.empty())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("No root bone found.");
        }
        else if (BoneChildren.size() != Bones.size())
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("Bone hierarchy cache is invalid. Bones: %d, Children: %d",
                                static_cast<int32>(Bones.size()), static_cast<int32>(BoneChildren.size()));
        }
        else
        {
            for (int32 RootBoneIndex : RootBoneIndices)
            {
                DrawBoneTreeNode(Bones, BoneChildren, RootBoneIndex, 0, State, SelectionManager, VisibleBoneOrder);
            }
        }
    }

    FEditorUIStyle::EndSkeletonTreeTable();
    FPanel::End();
}

void FSkeletonTreePanel::DrawBoneTreeNode(const TArray<FBoneInfo>& Bones, const TArray<TArray<int32>>& BoneChildren,
                                          int32 BoneIndex, int32 Depth, FSkeletalMeshEditorState& State,
                                          FSkeletalMeshSelectionManager& SelectionManager,
                                          const TArray<int32>& VisibleBoneOrder)
{
    const int32 BoneCount = static_cast<int32>(Bones.size());
    const int32 ChildrenCount = static_cast<int32>(BoneChildren.size());

    if (BoneIndex < 0 || BoneIndex >= BoneCount || BoneIndex >= ChildrenCount)
    {
        return;
    }

    const bool bHasChildren = !BoneChildren[BoneIndex].empty();
    const bool bOpen = DrawBoneRow(Bones[BoneIndex], BoneIndex, Depth, bHasChildren, false, State, SelectionManager, VisibleBoneOrder);

    if (bHasChildren && bOpen)
    {
        for (int32 ChildBoneIndex : BoneChildren[BoneIndex])
        {
            DrawBoneTreeNode(Bones, BoneChildren, ChildBoneIndex, Depth + 1, State, SelectionManager, VisibleBoneOrder);
        }

        ImGui::TreePop();
    }
}

void FSkeletonTreePanel::DrawFilteredBoneList(const TArray<FBoneInfo>& Bones, FSkeletalMeshEditorState& State,
                                              FSkeletalMeshSelectionManager& SelectionManager,
                                              const TArray<int32>& VisibleBoneOrder)
{
    for (int32 BoneIndex : VisibleBoneOrder)
    {
        if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Bones.size()))
        {
            continue;
        }

        DrawBoneRow(Bones[BoneIndex], BoneIndex, 0, false, true, State, SelectionManager, VisibleBoneOrder);
    }
}

bool FSkeletonTreePanel::DrawBoneRow(const FBoneInfo& Bone, int32 BoneIndex, int32 Depth, bool bHasChildren,
                                     bool bFilteredList, FSkeletalMeshEditorState& State,
                                     FSkeletalMeshSelectionManager& SelectionManager,
                                     const TArray<int32>& VisibleBoneOrder)
{
    const bool bSelected = SelectionManager.IsSelected(BoneIndex);
    const char* BoneName = Bone.Name.empty() ? "(unnamed bone)" : Bone.Name.c_str();

    ImGui::TableNextRow(ImGuiTableRowFlags_None, FEditorUIStyle::SkeletonTreeRowHeight);
    ImGui::TableSetColumnIndex(0);

    ImGui::PushID(BoneIndex);

    const float CellStartScreenX = ImGui::GetCursorScreenPos().x;
    const float IndentWidth = FEditorUIStyle::SkeletonTreeIndentWidth * static_cast<float>(Depth);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + IndentWidth);

    ImGuiTreeNodeFlags Flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_DefaultOpen;

    if (bSelected)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (!bHasChildren || bFilteredList)
    {
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    char Label[256];
    snprintf(Label, sizeof(Label), "    %s", BoneName);

    FEditorUIStyle::PushSkeletonTreeRowStyle(bSelected);
    const bool bOpen = ImGui::TreeNodeEx("##BoneNode", Flags, "%s", Label);
    FEditorUIStyle::PopSkeletonTreeRowStyle();

    DrawBoneIconForLastTreeItem(Depth, CellStartScreenX, bSelected);
    FEditorUIStyle::DrawSkeletonTreeGuides(Depth, Depth > 0, CellStartScreenX);

    if (ImGui::IsItemClicked())
    {
        ApplyBoneClickSelection(BoneIndex, State, SelectionManager, VisibleBoneOrder);
    }

    // 선택된 행에서는 아이콘과 글자를 검정색으로 보이게 하기 위해 Text 색상을 검정으로 밀어 넣는다.
    // TreeNodeEx가 라벨을 직접 그리므로 별도 후처리는 필요 없다.

    ImGui::PopID();

    return bOpen;
}

void FSkeletonTreePanel::ApplyBoneClickSelection(int32 BoneIndex, FSkeletalMeshEditorState& State,
                                                 FSkeletalMeshSelectionManager& SelectionManager,
                                                 const TArray<int32>& VisibleBoneOrder)
{
    if (ImGui::GetIO().KeyShift)
    {
        SelectionManager.SelectRange(BoneIndex, VisibleBoneOrder);
    }
    else if (ImGui::GetIO().KeyCtrl)
    {
        SelectionManager.ToggleBone(BoneIndex);
    }
    else
    {
        SelectionManager.SelectBone(BoneIndex);
    }

    SyncLegacySelectedBoneIndex(State, SelectionManager);
}

void FSkeletonTreePanel::SyncLegacySelectedBoneIndex(FSkeletalMeshEditorState& State,
                                                     const FSkeletalMeshSelectionManager& SelectionManager)
{
    State.SelectedBoneIndex = SelectionManager.GetPrimaryBoneIndex();
}
