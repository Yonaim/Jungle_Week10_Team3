#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"

#include "Common/UI/EditorDockLayoutUtils.h"
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
    // IAssetEditor 기본 wrapper 경로에서 호출될 때를 위한 fallback.
    // 실제 Asset Editor 통합 경로에서는 UsesExternalPanels() == true이므로
    // FAssetEditorTabManager가 RenderPanels()를 직접 호출한다.
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

    bCapturingInput = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
                      ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
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

FEditorPanelDesc FSkeletalMeshEditor::MakePanelDesc(const char *DisplayName, const char *StableName, const char *IconKey,
                                                    ImGuiWindowFlags Flags) const
{
    FEditorPanelDesc Desc;
    Desc.DisplayName = DisplayName;
    Desc.IconKey = IconKey;
    Desc.WindowFlags = Flags;
    Desc.bClosable = false;
    Desc.bApplyContentTopInset = true;
    Desc.bApplySideInset = true;
    Desc.bApplyBottomInset = true;

    // StableId는 Render 함수 안에서 std::string 수명을 유지한 채 c_str()로 넣는다.
    (void)StableName;
    return Desc;
}

void FSkeletalMeshEditor::BuildDefaultDockLayout(ImGuiID DockspaceId)
{
    if (DockspaceId == 0)
    {
        return;
    }

    const std::string ToolbarId = MakePanelStableId("Toolbar");
    const std::string SkeletonId = MakePanelStableId("SkeletonTree");
    const std::string PreviewId = MakePanelStableId("PreviewViewport");
    const std::string DetailsId = MakePanelStableId("Details");

    FEditorPanelDesc ToolbarDesc = MakePanelDesc("Toolbar", "Toolbar", nullptr);
    ToolbarDesc.StableId = ToolbarId.c_str();

    FEditorPanelDesc SkeletonDesc = MakePanelDesc("Skeleton Tree", "SkeletonTree", "Editor.Icon.Panel.Outliner");
    SkeletonDesc.StableId = SkeletonId.c_str();

    FEditorPanelDesc PreviewDesc = MakePanelDesc("Preview Viewport", "PreviewViewport", "Editor.Icon.Panel.Viewport");
    PreviewDesc.StableId = PreviewId.c_str();

    FEditorPanelDesc DetailsDesc = MakePanelDesc("Details", "Details", "Editor.Icon.Panel.Details");
    DetailsDesc.StableId = DetailsId.c_str();

    FFourPanelDockLayoutDesc LayoutDesc;
    LayoutDesc.ToolbarWindow = FEditorPanel::MakeTitle(ToolbarDesc);
    LayoutDesc.LeftWindow = FEditorPanel::MakeTitle(SkeletonDesc);
    LayoutDesc.CenterWindow = FEditorPanel::MakeTitle(PreviewDesc);
    LayoutDesc.RightWindow = FEditorPanel::MakeTitle(DetailsDesc);

    FEditorDockLayoutUtils::DockFourPanelLayout(DockspaceId, LayoutDesc);
}

void FSkeletalMeshEditor::RenderPanelsInternal(float DeltaTime, ImGuiID DockspaceId)
{
    (void)DockspaceId;

    const std::string ToolbarId = MakePanelStableId("Toolbar");
    const std::string SkeletonId = MakePanelStableId("SkeletonTree");
    const std::string PreviewId = MakePanelStableId("PreviewViewport");
    const std::string DetailsId = MakePanelStableId("Details");

    FEditorPanelDesc ToolbarDesc = MakePanelDesc("Toolbar", "Toolbar", nullptr,
                                                  ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                                      ImGuiWindowFlags_NoScrollWithMouse);
    ToolbarDesc.StableId = ToolbarId.c_str();
    ToolbarDesc.bDrawTitleIcon = false;
    ToolbarDesc.bApplyContentTopInset = true;
    ToolbarDesc.bApplySideInset = true;
    ToolbarDesc.bApplyBottomInset = false;

    FEditorPanelDesc SkeletonDesc = MakePanelDesc("Skeleton Tree", "SkeletonTree", "Editor.Icon.Panel.Outliner");
    SkeletonDesc.StableId = SkeletonId.c_str();

    FEditorPanelDesc PreviewDesc = MakePanelDesc("Preview Viewport", "PreviewViewport", "Editor.Icon.Panel.Viewport",
                                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                                     ImGuiWindowFlags_NoScrollWithMouse);
    PreviewDesc.StableId = PreviewId.c_str();
    PreviewDesc.bApplySideInset = false;
    PreviewDesc.bApplyBottomInset = false;

    FEditorPanelDesc DetailsDesc = MakePanelDesc("Details", "Details", "Editor.Icon.Panel.Details");
    DetailsDesc.StableId = DetailsId.c_str();

    RenderToolbarPanel(ToolbarDesc);
    SkeletonTreePanel.Render(EditingAsset, State, SkeletonDesc);
    PreviewViewport.Render(EditingAsset, State, DeltaTime, PreviewDesc);
    DetailsPanel.Render(EditingAsset, EditingAssetPath, State, DetailsDesc);
}

void FSkeletalMeshEditor::RenderToolbarPanel(const FEditorPanelDesc &PanelDesc)
{
    if (FEditorPanel::Begin(PanelDesc))
    {
        Toolbar.Render(EditingAsset, State);
    }
    FEditorPanel::End();
}
