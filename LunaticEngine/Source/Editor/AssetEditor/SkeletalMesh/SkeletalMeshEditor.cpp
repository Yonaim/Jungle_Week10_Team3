#include "PCH/LunaticPCH.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"

#include "Common/UI/Docking/DockLayoutUtils.h"
#include "Core/Notification.h"
#include "Component/SkeletalMeshComponent.h"
#include "EditorEngine.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Materials/MaterialManager.h"
#include "Mesh/MeshAssetManager.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <string>
#include <utility>

namespace
{
uint32 GNextSkeletalMeshEditorId = 1;

}

void FSkeletalMeshEditor::Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;

    if (EditorInstanceId == 0)
    {
        EditorInstanceId = GNextSkeletalMeshEditorId++;
    }

    PreviewViewport.Initialize(EditorEngine ? EditorEngine->GetWindow() : nullptr, Renderer);
}

bool FSkeletalMeshEditor::OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath)
{
    USkeletalMesh *SkeletalMesh = Cast<USkeletalMesh>(Asset);
    if (!SkeletalMesh)
    {
        return false;
    }

    Close();

    EditingAsset = SkeletalMesh;
    EditingAssetPath = AssetPath.lexically_normal();
    if (FSkeletalMesh *MeshData = EditingAsset->GetSkeletalMeshAsset())
    {
        if (MeshData->PathFileName.empty())
        {
            MeshData->PathFileName = FPaths::ToUtf8(EditingAssetPath.generic_wstring());
        }
        MeshData->BuildBoneHierarchyCache();
    }

    State = FSkeletalMeshEditorState{};
    SelectionManager.Reset();
    UndoStack.clear();
    RedoStack.clear();
    BuiltDockspaceId = 0;

    bPreviewPanelOpen = true;
    bSkeletonTreePanelOpen = true;
    bDetailsPanelOpen = true;
    bBoneDetailsPanelOpen = true;

    bOpen = true;
    bDirty = false;

    FNotificationManager::Get().AddNotification("Opened asset: " + FPaths::ToUtf8(EditingAssetPath.filename().wstring()),
                                                ENotificationType::Success, 3.0f);
    return true;
}

void FSkeletalMeshEditor::Close()
{
    if (EditingAsset)
    {
        UObjectManager::Get().DestroyObject(EditingAsset);
        EditingAsset = nullptr;
    }

    EditingAssetPath.clear();
    State = FSkeletalMeshEditorState{};
    SelectionManager.Reset();
    UndoStack.clear();
    RedoStack.clear();
    BuiltDockspaceId = 0;

    bPreviewPanelOpen = true;
    bSkeletonTreePanelOpen = true;
    bDetailsPanelOpen = true;
    bBoneDetailsPanelOpen = true;

    bOpen = false;
    bDirty = false;
    bCapturingInput = false;
}

bool FSkeletalMeshEditor::Save()
{
    if (!EditingAsset)
    {
        FNotificationManager::Get().AddNotification("No Skeletal Mesh asset is open.", ENotificationType::Info, 3.0f);
        return false;
    }

    if (EditingAssetPath.empty())
    {
        FNotificationManager::Get().AddNotification("Cannot save Skeletal Mesh: asset path is empty.", ENotificationType::Error, 4.0f);
        return false;
    }

    FString Error;
    if (!FAssetFileSerializer::SaveObjectToAssetFile(EditingAssetPath, EditingAsset, &Error))
    {
        FNotificationManager::Get().AddNotification(
            Error.empty() ? "Failed to save Skeletal Mesh asset." : Error,
            ENotificationType::Error,
            4.0f);
        return false;
    }

    FMeshAssetManager::MarkAssetCacheStale(FPaths::ToUtf8(EditingAssetPath.generic_wstring()));

    bDirty = false;
    FNotificationManager::Get().AddNotification(
        "Saved asset: " + FPaths::ToUtf8(EditingAssetPath.filename().wstring()),
        ENotificationType::Success,
        3.0f);
    return true;
}

void FSkeletalMeshEditor::Tick(float DeltaTime)
{
    PreviewViewport.Tick(DeltaTime);
}

void FSkeletalMeshEditor::RenderContent(float DeltaTime)
{
    RenderPanels(DeltaTime, 0);
}

void FSkeletalMeshEditor::InvalidateDockLayout()
{
    BuiltDockspaceId = 0;
}

void FSkeletalMeshEditor::RenderPanels(float DeltaTime, ImGuiID DockspaceId)
{
    if (!bOpen)
    {
        bCapturingInput = false;
        return;
    }

    if (DockspaceId != 0 && BuiltDockspaceId != DockspaceId)
    {
        BuildDefaultDockLayout(DockspaceId);
        BuiltDockspaceId = DockspaceId;
    }

    RenderPanelsInternal(DeltaTime, DockspaceId);

    FSkeletalMeshPreviewViewportClient *PreviewClient = PreviewViewport.GetViewportClient();
    bCapturingInput = PreviewClient && (PreviewClient->IsHovered() || PreviewClient->IsActive());
}

FEditorViewportClient *FSkeletalMeshEditor::GetActiveViewportClient()
{
    if (!bOpen || !bPreviewPanelOpen)
    {
        return nullptr;
    }

    return PreviewViewport.GetViewportClient();
}

void FSkeletalMeshEditor::CollectViewportClients(TArray<FEditorViewportClient *> &OutClients)
{
    if (!bOpen)
    {
        return;
    }

    if (FSkeletalMeshPreviewViewportClient *Client = PreviewViewport.GetViewportClient())
    {
        OutClients.push_back(Client);
    }
}

void FSkeletalMeshEditor::BuildWindowMenu()
{
    if (ImGui::MenuItem("Preview Viewport", nullptr, bPreviewPanelOpen))
        bPreviewPanelOpen = !bPreviewPanelOpen;
    if (ImGui::MenuItem("Skeleton Tree", nullptr, bSkeletonTreePanelOpen))
        bSkeletonTreePanelOpen = !bSkeletonTreePanelOpen;
    if (ImGui::MenuItem("Asset Details", nullptr, bDetailsPanelOpen))
        bDetailsPanelOpen = !bDetailsPanelOpen;
    if (ImGui::MenuItem("Details", nullptr, bBoneDetailsPanelOpen))
        bBoneDetailsPanelOpen = !bBoneDetailsPanelOpen;

    ImGui::Separator();
    if (ImGui::MenuItem("Reset Skeletal Mesh Editor Layout"))
    {
        bPreviewPanelOpen = true;
        bSkeletonTreePanelOpen = true;
        bDetailsPanelOpen = true;
        bBoneDetailsPanelOpen = true;
        BuiltDockspaceId = 0;
    }
}

void FSkeletalMeshEditor::BuildCustomMenus()
{
    if (ImGui::BeginMenu("Mesh"))
    {
        ImGui::MenuItem("Reimport", nullptr, false, false);
        ImGui::MenuItem("Reset Preview", nullptr, false, EditingAsset != nullptr);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Skeleton"))
    {
        ImGui::MenuItem("Show Bones", nullptr, &State.bShowBones, EditingAsset != nullptr);
        ImGui::Separator();
        ImGui::MenuItem("Pose Edit Mode", nullptr, &State.bEnablePoseEditMode, EditingAsset != nullptr);
        ImGui::MenuItem("Bone Gizmo", nullptr, false, false);
        ImGui::EndMenu();
    }
}

bool FSkeletalMeshEditor::CanUndo() const
{
    return !UndoStack.empty();
}

bool FSkeletalMeshEditor::CanRedo() const
{
    return !RedoStack.empty();
}

void FSkeletalMeshEditor::Undo()
{
    if (!CanUndo())
    {
        return;
    }

    FHistoryState CurrentState = CaptureHistoryState();
    FHistoryState PreviousState = UndoStack.back();
    UndoStack.pop_back();

    RedoStack.push_back(std::move(CurrentState));
    ApplyHistoryState(PreviousState);
    bDirty = true;
}

void FSkeletalMeshEditor::Redo()
{
    if (!CanRedo())
    {
        return;
    }

    FHistoryState CurrentState = CaptureHistoryState();
    FHistoryState NextState = RedoStack.back();
    RedoStack.pop_back();

    UndoStack.push_back(std::move(CurrentState));
    ApplyHistoryState(NextState);
    bDirty = true;
}

FSkeletalMeshEditor::FHistoryState FSkeletalMeshEditor::CaptureHistoryState() const
{
    FHistoryState StateSnapshot;
    if (!EditingAsset)
    {
        return StateSnapshot;
    }

    const TArray<FStaticMaterial> &Materials = EditingAsset->GetStaticMaterials();
    StateSnapshot.MaterialPaths.reserve(Materials.size());
    for (const FStaticMaterial &Material : Materials)
    {
        StateSnapshot.MaterialPaths.push_back(Material.MaterialInterface
            ? Material.MaterialInterface->GetAssetPathFileName()
            : FString("None"));
    }
    return StateSnapshot;
}

void FSkeletalMeshEditor::ApplyHistoryState(const FHistoryState &HistoryState)
{
    if (!EditingAsset)
    {
        return;
    }

    TArray<FStaticMaterial> &Materials = EditingAsset->GetStaticMaterialsMutable();
    const int32 ApplyCount = (std::min)(static_cast<int32>(Materials.size()), static_cast<int32>(HistoryState.MaterialPaths.size()));
    USkeletalMeshComponent *PreviewComponent = GetPreviewComponent();

    for (int32 MaterialIndex = 0; MaterialIndex < ApplyCount; ++MaterialIndex)
    {
        const FString &MaterialPath = HistoryState.MaterialPaths[MaterialIndex];
        UMaterial *Material = nullptr;
        if (!MaterialPath.empty() && MaterialPath != "None")
        {
            Material = FMaterialManager::Get().GetOrCreateMaterial(MaterialPath);
        }

        Materials[MaterialIndex].MaterialInterface = Material;
        if (PreviewComponent)
        {
            PreviewComponent->SetMaterial(MaterialIndex, Material);
        }
    }
}

bool FSkeletalMeshEditor::AreHistoryStatesEqual(const FHistoryState &A, const FHistoryState &B) const
{
    if (A.MaterialPaths.size() != B.MaterialPaths.size())
    {
        return false;
    }

    for (size_t Index = 0; Index < A.MaterialPaths.size(); ++Index)
    {
        if (A.MaterialPaths[Index] != B.MaterialPaths[Index])
        {
            return false;
        }
    }
    return true;
}

void FSkeletalMeshEditor::PushUndoStateIfChanged(const FHistoryState &BeforeState, const FHistoryState &AfterState)
{
    if (AreHistoryStatesEqual(BeforeState, AfterState))
    {
        return;
    }

    UndoStack.push_back(BeforeState);
    RedoStack.clear();
    bDirty = true;
}

USkeletalMeshComponent *FSkeletalMeshEditor::GetPreviewComponent() const
{
    FSkeletalMeshPreviewViewportClient *PreviewClient = const_cast<FSkeletalMeshPreviewViewport &>(PreviewViewport).GetViewportClient();
    return PreviewClient ? PreviewClient->GetPreviewComponent() : nullptr;
}

std::string FSkeletalMeshEditor::MakePanelStableId(const char *PanelName) const
{
    return std::string("SkeletalMeshEditor_") + std::to_string(EditorInstanceId) + "_" + PanelName;
}

FPanelDesc FSkeletalMeshEditor::MakePanelDesc(const char *DisplayName, const char *StableName, const char *IconKey,
                                                    ImGuiWindowFlags Flags) const
{
    FPanelDesc Desc;
    Desc.DisplayName = DisplayName;
    Desc.IconKey = IconKey;
    Desc.WindowFlags = Flags;
    Desc.bClosable = true;
    Desc.bApplyContentTopInset = true;
    Desc.bApplySideInset = true;
    Desc.bApplyBottomInset = true;

    (void)StableName;
    return Desc;
}

void FSkeletalMeshEditor::BuildDefaultDockLayout(ImGuiID DockspaceId)
{
    if (DockspaceId == 0)
    {
        return;
    }

    const std::string SkeletonId = MakePanelStableId("SkeletonTree");
    const std::string PreviewId = MakePanelStableId("PreviewViewport");
    const std::string DetailsId = MakePanelStableId("AssetDetails");
    const std::string BoneDetailsId = MakePanelStableId("Details");

    FPanelDesc SkeletonDesc = MakePanelDesc("Skeleton Tree", "SkeletonTree", "Editor.Icon.Panel.SkeletonTree");
    SkeletonDesc.StableId = SkeletonId.c_str();
    SkeletonDesc.bOpen = &bSkeletonTreePanelOpen;

    const std::string PreviewTitle = std::string("Viewport ") + std::to_string(EditorInstanceId);
    FPanelDesc PreviewDesc = MakePanelDesc(PreviewTitle.c_str(), "PreviewViewport", "Editor.Icon.Panel.Viewport");
    PreviewDesc.StableId = PreviewId.c_str();

    FPanelDesc DetailsDesc = MakePanelDesc("Asset Details", "AssetDetails", "Editor.Icon.SkeletalMesh");
    DetailsDesc.StableId = DetailsId.c_str();

    FPanelDesc BoneDetailsDesc = MakePanelDesc("Details", "Details", "Editor.Icon.Panel.Details");
    BoneDetailsDesc.StableId = BoneDetailsId.c_str();

    FAssetPreviewDockLayoutDesc LayoutDesc;
    LayoutDesc.CenterWindow = FPanel::MakeTitle(PreviewDesc);
    LayoutDesc.RightTopWindow = FPanel::MakeTitle(SkeletonDesc);
    LayoutDesc.RightBottomWindow = FPanel::MakeTitle(DetailsDesc);
    LayoutDesc.RightBottomSecondWindow = FPanel::MakeTitle(BoneDetailsDesc);

    FDockLayoutUtils::DockAssetPreviewLayout(DockspaceId, LayoutDesc);
}

void FSkeletalMeshEditor::RenderPanelsInternal(float DeltaTime, ImGuiID DockspaceId)
{
    (void)DockspaceId;

    const std::string SkeletonId = MakePanelStableId("SkeletonTree");
    const std::string PreviewId = MakePanelStableId("PreviewViewport");
    const std::string DetailsId = MakePanelStableId("AssetDetails");
    const std::string BoneDetailsId = MakePanelStableId("Details");

    FPanelDesc SkeletonDesc = MakePanelDesc("Skeleton Tree", "SkeletonTree", "Editor.Icon.Panel.SkeletonTree");
    SkeletonDesc.StableId = SkeletonId.c_str();
    SkeletonDesc.bOpen = &bSkeletonTreePanelOpen;

    const std::string PreviewTitle = std::string("Viewport ") + std::to_string(EditorInstanceId);
    FPanelDesc PreviewDesc = MakePanelDesc(PreviewTitle.c_str(), "PreviewViewport", "Editor.Icon.Panel.Viewport",
                                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                                     ImGuiWindowFlags_NoScrollWithMouse);
    PreviewDesc.StableId = PreviewId.c_str();
    PreviewDesc.bOpen = &bPreviewPanelOpen;
    PreviewDesc.bApplySideInset = false;
    PreviewDesc.bApplyBottomInset = false;

    FPanelDesc DetailsDesc = MakePanelDesc("Asset Details", "AssetDetails", "Editor.Icon.SkeletalMesh");
    DetailsDesc.StableId = DetailsId.c_str();
    DetailsDesc.bOpen = &bDetailsPanelOpen;

    FPanelDesc BoneDetailsDesc = MakePanelDesc("Details", "Details", "Editor.Icon.Panel.Details");
    BoneDetailsDesc.StableId = BoneDetailsId.c_str();
    BoneDetailsDesc.bOpen = &bBoneDetailsPanelOpen;

    if (bPreviewPanelOpen)
    {
        PreviewViewport.Render(EditingAsset, State, &SelectionManager, &Toolbar, DeltaTime, PreviewDesc);
    }
    if (bSkeletonTreePanelOpen)
    {
        SkeletonTreePanel.Render(EditingAsset, State, SelectionManager, SkeletonDesc);
    }
    if (bDetailsPanelOpen)
    {
        USkeletalMeshComponent *PreviewComponent = GetPreviewComponent();
        const FHistoryState BeforeState = CaptureHistoryState();

        AssetDetailsPanel.RenderSkeletalMesh(EditingAsset, PreviewComponent, EditingAssetPath, State,
                                             SelectionManager, DetailsDesc);

        const FHistoryState AfterState = CaptureHistoryState();
        PushUndoStateIfChanged(BeforeState, AfterState);
    }
    if (bBoneDetailsPanelOpen)
    {
        USkeletalMeshComponent *PreviewComponent = GetPreviewComponent();
        DetailsPanel.Render(EditingAsset, PreviewComponent, State, SelectionManager, BoneDetailsDesc);
    }
}
