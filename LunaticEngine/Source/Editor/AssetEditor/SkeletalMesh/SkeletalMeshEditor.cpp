#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"

#include "Common/UI/Docking/DockLayoutUtils.h"
#include "Core/Notification.h"
#include "EditorEngine.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <string>

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
    State = FSkeletalMeshEditorState{};
    BuiltDockspaceId = 0;

    bPreviewPanelOpen = true;
    bSkeletonTreePanelOpen = true;
    bDetailsPanelOpen = true;

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
    BuiltDockspaceId = 0;

    bPreviewPanelOpen = true;
    bSkeletonTreePanelOpen = true;
    bDetailsPanelOpen = true;

    bOpen = false;
    bDirty = false;
    bCapturingInput = false;
}

bool FSkeletalMeshEditor::Save()
{
    FNotificationManager::Get().AddNotification("Skeletal Mesh Viewer is read-only for now.", ENotificationType::Info, 3.0f);
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
    ImGui::Checkbox("Preview Viewport", &bPreviewPanelOpen);
    ImGui::Checkbox("Skeleton Tree", &bSkeletonTreePanelOpen);
    ImGui::Checkbox("Details", &bDetailsPanelOpen);

    ImGui::Separator();
    if (ImGui::MenuItem("Reset Skeletal Mesh Editor Layout"))
    {
        bPreviewPanelOpen = true;
        bSkeletonTreePanelOpen = true;
        bDetailsPanelOpen = true;
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
    const std::string DetailsId = MakePanelStableId("Details");

    FPanelDesc SkeletonDesc = MakePanelDesc("Skeleton Tree", "SkeletonTree", "Editor.Icon.Panel.Outliner");
    SkeletonDesc.StableId = SkeletonId.c_str();
    SkeletonDesc.bOpen = &bSkeletonTreePanelOpen;

    FPanelDesc PreviewDesc = MakePanelDesc("Preview Viewport", "PreviewViewport", "Editor.Icon.Panel.Viewport");
    PreviewDesc.StableId = PreviewId.c_str();

    FPanelDesc DetailsDesc = MakePanelDesc("Details", "Details", "Editor.Icon.Panel.Details");
    DetailsDesc.StableId = DetailsId.c_str();

    FAssetPreviewDockLayoutDesc LayoutDesc;
    LayoutDesc.CenterWindow = FPanel::MakeTitle(PreviewDesc);
    LayoutDesc.RightTopWindow = FPanel::MakeTitle(SkeletonDesc);
    LayoutDesc.RightBottomWindow = FPanel::MakeTitle(DetailsDesc);

    FDockLayoutUtils::DockAssetPreviewLayout(DockspaceId, LayoutDesc);
}

void FSkeletalMeshEditor::RenderPanelsInternal(float DeltaTime, ImGuiID DockspaceId)
{
    (void)DockspaceId;

    const std::string SkeletonId = MakePanelStableId("SkeletonTree");
    const std::string PreviewId = MakePanelStableId("PreviewViewport");
    const std::string DetailsId = MakePanelStableId("Details");

    FPanelDesc SkeletonDesc = MakePanelDesc("Skeleton Tree", "SkeletonTree", "Editor.Icon.Panel.Outliner");
    SkeletonDesc.StableId = SkeletonId.c_str();
    SkeletonDesc.bOpen = &bSkeletonTreePanelOpen;

    FPanelDesc PreviewDesc = MakePanelDesc("Preview Viewport", "PreviewViewport", "Editor.Icon.Panel.Viewport",
                                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                                     ImGuiWindowFlags_NoScrollWithMouse);
    PreviewDesc.StableId = PreviewId.c_str();
    PreviewDesc.bOpen = &bPreviewPanelOpen;
    PreviewDesc.bApplySideInset = false;
    PreviewDesc.bApplyBottomInset = false;

    FPanelDesc DetailsDesc = MakePanelDesc("Details", "Details", "Editor.Icon.Panel.Details");
    DetailsDesc.StableId = DetailsId.c_str();
    DetailsDesc.bOpen = &bDetailsPanelOpen;

    if (bPreviewPanelOpen)
    {
        PreviewViewport.Render(EditingAsset, State, &Toolbar, DeltaTime, PreviewDesc);
    }
    if (bSkeletonTreePanelOpen)
    {
        SkeletonTreePanel.Render(EditingAsset, State, SkeletonDesc);
    }
    if (bDetailsPanelOpen)
    {
        DetailsPanel.Render(EditingAsset, EditingAssetPath, State, DetailsDesc);
    }
}
