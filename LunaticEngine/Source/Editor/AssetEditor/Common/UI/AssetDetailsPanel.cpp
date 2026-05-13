#include "PCH/LunaticPCH.h"
#include "AssetEditor/Common/UI/AssetDetailsPanel.h"

#include "AssetEditor/SkeletalMesh/Selection/SkeletalMeshSelectionManager.h"

#include "Component/SkeletalMeshComponent.h"
#include "Common/UI/Details/EditorDetailsWidgets.h"
#include "Common/UI/Style/EditorUIStyle.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Platform/Paths.h"
#include "Texture/Texture2D.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>

namespace
{
const char *PreviewModeToText(ESkeletalMeshPreviewMode Mode)
{
    switch (Mode)
    {
    case ESkeletalMeshPreviewMode::ReferencePose:
        return "Reference Pose";
    case ESkeletalMeshPreviewMode::SkinnedPose:
        return "Skinned Pose";
    default:
        return "Unknown";
    }
}

const char *SafeText(const FString &Text)
{
    return Text.empty() ? "None" : Text.c_str();
}

void DrawSmallIconButton(const char *Id, const char *Label, const char *Tooltip)
{
    FEditorUIStyle::PushHeaderButtonStyle(4.0f);
    ImGui::Button(Id, ImVec2(22.0f, 22.0f));
    FEditorUIStyle::PopHeaderButtonStyle();
    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 LabelSize = ImGui::CalcTextSize(Label);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(Min.x + (22.0f - LabelSize.x) * 0.5f, Min.y + (22.0f - LabelSize.y) * 0.5f),
        ImGui::GetColorU32(ImGuiCol_Text), Label);
    if (Tooltip && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", Tooltip);
    }
}
} // namespace

bool FAssetDetailsPanel::RenderSkeletalMesh(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent,
                                            const std::filesystem::path &AssetPath, FSkeletalMeshEditorState &State,
                                            FSkeletalMeshSelectionManager &SelectionManager,
                                            const FPanelDesc &PanelDesc)
{
    if (!FPanel::Begin(PanelDesc))
    {
        FPanel::End();
        return false;
    }

    RenderSearchToolbar();

    if (!Mesh)
    {
        ImGui::TextDisabled("No SkeletalMesh asset selected.");
        FPanel::End();
        return false;
    }

    bool bChanged = false;
    bChanged |= RenderMaterialSlots(Mesh, PreviewComponent, State);
    ImGui::Spacing();
    RenderMeshInfo(Mesh, AssetPath, State, SelectionManager);
    ImGui::Spacing();
    RenderViewerActions(State, SelectionManager);
    ImGui::Spacing();
    RenderBoneSelectionSummary(State, SelectionManager);

    FPanel::End();
    return bChanged;
}

void FAssetDetailsPanel::RenderSearchToolbar()
{
    static char SearchBuffer[128] = {};
    FEditorDetailsWidgets::DrawDetailsSearchToolbar("##AssetDetailsSearch", SearchBuffer, sizeof(SearchBuffer));
}

bool FAssetDetailsPanel::RenderMaterialSlots(USkeletalMesh *Mesh, USkeletalMeshComponent *PreviewComponent,
                                             FSkeletalMeshEditorState &State)
{
    if (!FEditorDetailsWidgets::BeginSection("Material Slots"))
    {
        return false;
    }

    TArray<FStaticMaterial> &Materials = Mesh->GetStaticMaterialsMutable();
    const int32 MaterialCount = static_cast<int32>(Materials.size());
    bool bChangedAnySlot = false;

    if (FEditorDetailsWidgets::BeginReadOnlyTable("##AssetDetailsMaterialSlotSummary"))
    {
        FEditorDetailsWidgets::DrawReadOnlyIntRow("Material Slots", MaterialCount);
        ImGui::EndTable();
    }

    if (MaterialCount <= 0)
    {
        ImGui::TextDisabled("No material slots.");
        return false;
    }

    ImGui::Spacing();
    for (int32 Index = 0; Index < MaterialCount; ++Index)
    {
        const FStaticMaterial &StaticSlot = Materials[Index];
        FMaterialSlot Slot;
        Slot.Path = StaticSlot.MaterialInterface ? StaticSlot.MaterialInterface->GetAssetPathFileName() : "None";

        ImGui::PushID(Index);
        const bool bChanged = FEditorDetailsWidgets::DrawMaterialSlotRow({
            Index,
            StaticSlot.MaterialSlotName.empty() ? "None" : StaticSlot.MaterialSlotName.c_str(),
            &Slot,
            nullptr,
            false,
            true,
            120.0f,
        });
        if (bChanged)
        {
            UMaterial *NewMaterial =
                (Slot.Path.empty() || Slot.Path == "None") ? nullptr : FMaterialManager::Get().GetOrCreateMaterial(Slot.Path);
            StaticSlot.MaterialInterface = NewMaterial;
            if (PreviewComponent)
            {
                PreviewComponent->SetMaterial(Index, NewMaterial);
            }
            bChangedAnySlot = true;
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            State.SelectedMaterialSlotIndex = Index;
        }

        if (Index + 1 < MaterialCount)
        {
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    return bChangedAnySlot;
}

void FAssetDetailsPanel::RenderMeshInfo(USkeletalMesh *Mesh, const std::filesystem::path &AssetPath,
                                        FSkeletalMeshEditorState &State,
                                        FSkeletalMeshSelectionManager &SelectionManager)
{
    if (!FEditorUIStyle::BeginDetailsSection("Mesh Info"))
    {
        return;
    }

    const FString FileName = AssetPath.empty() ? FString("Untitled") : FPaths::ToUtf8(AssetPath.filename().wstring());
    const FString FullPath = AssetPath.empty() ? FString("") : FPaths::ToUtf8(AssetPath.wstring());

    if (FEditorUIStyle::BeginDetailsReadOnlyTable("##AssetDetailsSkeletalMeshInfoTable"))
    {
        FEditorUIStyle::DrawReadOnlyTextRow("Asset", FileName.c_str());
        if (!FullPath.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", FullPath.c_str());
        }

        FEditorUIStyle::DrawReadOnlyIntRow("Bone Count", Mesh->GetBoneCount());
        FEditorUIStyle::DrawReadOnlyIntRow("Vertex Count", Mesh->GetVertexCount());
        FEditorUIStyle::DrawReadOnlyIntRow("Index Count", Mesh->GetIndexCount());
        FEditorUIStyle::DrawReadOnlyTextRow("Preview Mode", PreviewModeToText(State.PreviewMode));
        ImGui::EndTable();
    }
}

void FAssetDetailsPanel::RenderViewerActions(FSkeletalMeshEditorState &State, FSkeletalMeshSelectionManager &SelectionManager)
{
    if (!FEditorUIStyle::BeginDetailsSection("Viewer Actions"))
    {
        return;
    }

    FEditorUIStyle::PushHeaderButtonStyle();
    if (ImGui::Button("Frame Mesh"))
    {
        State.bFramePreviewRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Bone Selection"))
    {
        SelectionManager.ClearSelection();
        State.SelectedBoneIndex = -1;
    }
    FEditorUIStyle::PopHeaderButtonStyle();

    ImGui::Checkbox("Show Mesh Stats Overlay", &State.bShowMeshStatsOverlay);
    ImGui::Checkbox("Show Bones", &State.bShowBones);
}

void FAssetDetailsPanel::RenderBoneSelectionSummary(FSkeletalMeshEditorState &State,
                                                    FSkeletalMeshSelectionManager &SelectionManager)
{
    if (!FEditorUIStyle::BeginDetailsSection("Bone Editing"))
    {
        return;
    }

    ImGui::Checkbox("Pose Edit Mode", &State.bEnablePoseEditMode);

    if (FEditorUIStyle::BeginDetailsReadOnlyTable("##AssetDetailsBoneSelectionSummary"))
    {
        FEditorUIStyle::DrawReadOnlyIntRow("Primary Bone Index", SelectionManager.GetPrimaryBoneIndex());
        FEditorUIStyle::DrawReadOnlyIntRow("Selected Bone Count", SelectionManager.GetSelectedCount());
        ImGui::EndTable();
    }
}
