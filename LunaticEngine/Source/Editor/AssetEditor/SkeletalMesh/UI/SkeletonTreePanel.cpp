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

void FSkeletonTreePanel::Render(
	USkeletalMesh* Mesh,
	FSkeletalMeshEditorState& State,
	const FEditorPanelDesc& PanelDesc)
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
	ImGui::InputTextWithHint(
		"##SkeletalBoneSearch",
		"Search bone...",
		SearchBuffer,
		sizeof(SearchBuffer)
	);

	ImGui::Separator();

	if (BoneCount <= 0)
	{
		ImGui::TextDisabled("This mesh has no bone data.");
		ImGui::TextDisabled("FBX Importer가 skeleton/bone 데이터를 채우면 여기에 표시된다.");
		FEditorPanel::End();
		return;
	}

	const TArray<FBoneInfo>& Bones = Mesh->GetBones();

	if (Bones.empty())
	{
		ImGui::TextDisabled("Bone count is valid, but bone array is empty.");
		FEditorPanel::End();
		return;
	}

	if (State.SelectedBoneIndex >= static_cast<int32>(Bones.size()))
	{
		State.SelectedBoneIndex = -1;
	}

	if (ImGui::BeginChild("##SkeletalBoneTree", ImVec2(0, 0), true))
	{
		const bool bHasSearch = SearchBuffer[0] != '\0';

		if (bHasSearch)
		{
			DrawFilteredBoneList(Bones, State);
		}
		else
		{
			const TArray<TArray<int32>>& BoneChildren = Mesh->GetBoneChildren();
			const TArray<int32>& RootBoneIndices = Mesh->GetRootBoneIndices();

			if (RootBoneIndices.empty())
			{
				ImGui::TextDisabled("No root bone found.");
				ImGui::TextDisabled("BuildBoneHierarchyCache() may not have been called.");
			}
			else if (BoneChildren.size() != Bones.size())
			{
				ImGui::TextDisabled("Bone hierarchy cache is invalid.");
				ImGui::Text(
					"Bones: %d, Children: %d",
					static_cast<int32>(Bones.size()),
					static_cast<int32>(BoneChildren.size())
				);
			}
			else
			{
				for (int32 RootBoneIndex : RootBoneIndices)
				{
					DrawBoneTreeNode(Bones, BoneChildren, RootBoneIndex, State);
				}
			}
		}
	}

	ImGui::EndChild();

	FEditorPanel::End();
}

void FSkeletonTreePanel::DrawBoneTreeNode(
	const TArray<FBoneInfo>& Bones,
	const TArray<TArray<int32>>& BoneChildren,
	int32 BoneIndex,
	FSkeletalMeshEditorState& State)
{
	const int32 BoneCount = static_cast<int32>(Bones.size());
	const int32 ChildrenCount = static_cast<int32>(BoneChildren.size());

	if (BoneIndex < 0 || BoneIndex >= BoneCount)
	{
		return;
	}

	if (BoneIndex >= ChildrenCount)
	{
		return;
	}

	const FBoneInfo& Bone = Bones[BoneIndex];

	const bool bSelected = State.SelectedBoneIndex == BoneIndex;
	const bool bLeaf = BoneChildren[BoneIndex].empty();

	ImGuiTreeNodeFlags Flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (bSelected)
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (bLeaf)
	{
		Flags |= ImGuiTreeNodeFlags_Leaf;
		Flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	ImGui::PushID(BoneIndex);

	const char* BoneName = Bone.Name.empty()
		? "(unnamed bone)"
		: Bone.Name.c_str();

	const bool bOpen = ImGui::TreeNodeEx(
		"BoneNode",
		Flags,
		"%s [%d]",
		BoneName,
		BoneIndex
	);

	if (ImGui::IsItemClicked())
	{
		State.SelectedBoneIndex = BoneIndex;
	}

	if (!bLeaf && bOpen)
	{
		for (int32 ChildBoneIndex : BoneChildren[BoneIndex])
		{
			DrawBoneTreeNode(Bones, BoneChildren, ChildBoneIndex, State);
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void FSkeletonTreePanel::DrawFilteredBoneList(
	const TArray<FBoneInfo>& Bones,
	FSkeletalMeshEditorState& State)
{
	const int32 BoneCount = static_cast<int32>(Bones.size());

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const FBoneInfo& Bone = Bones[BoneIndex];

		const char* BoneName = Bone.Name.empty() ? "(unnamed bone)" : Bone.Name.c_str();

		if (!ContainsCaseInsensitive(BoneName, SearchBuffer))
		{
			continue;
		}

		char Label[256];
		snprintf(
			Label,
			sizeof(Label),
			"%s [%d]",
			BoneName,
			BoneIndex
		);

		const bool bSelected = State.SelectedBoneIndex == BoneIndex;

		if (ImGui::Selectable(Label, bSelected))
		{
			State.SelectedBoneIndex = BoneIndex;
		}
	}
}
