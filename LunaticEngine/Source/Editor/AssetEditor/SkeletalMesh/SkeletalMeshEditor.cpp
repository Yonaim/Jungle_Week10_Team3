#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"

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

std::string FSkeletalMeshEditor::MakePanelName(const char *BaseName) const
{
    return std::string(BaseName) + "###SkeletalMeshEditor_" + BaseName + "_" + std::to_string(EditorInstanceId);
}

void FSkeletalMeshEditor::BuildDefaultDockLayout(ImGuiID DockspaceId)
{
    if (DockspaceId == 0)
    {
        return;
    }

    // Level Editor의 메인 DockSpace 안에 SkeletalMesh Viewer 패널을 배치한다.
    // 기존 Level Editor 패널을 전부 지우면 안 되므로 DockBuilderRemoveNode()는 호출하지 않는다.
    // 첫 FBX / SkeletalMesh 오픈 시 왼쪽 Skeleton Tree, 중앙 Preview Viewport,
    // 오른쪽 Details, 상단 Toolbar가 자동으로 추가된다.
    ImGuiID MainId = DockspaceId;
    ImGuiID ToolbarId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Up, 0.10f, nullptr, &MainId);
    ImGuiID SkeletonId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Left, 0.22f, nullptr, &MainId);
    ImGuiID DetailsId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Right, 0.28f, nullptr, &MainId);
    ImGuiID PreviewId = MainId;

    ImGui::DockBuilderDockWindow(MakePanelName("Toolbar").c_str(), ToolbarId);
    ImGui::DockBuilderDockWindow(MakePanelName("Skeleton Tree").c_str(), SkeletonId);
    ImGui::DockBuilderDockWindow(MakePanelName("Preview Viewport").c_str(), PreviewId);
    ImGui::DockBuilderDockWindow(MakePanelName("Details").c_str(), DetailsId);
    ImGui::DockBuilderFinish(DockspaceId);
}


void FSkeletalMeshEditor::RenderPanelsInternal(float DeltaTime, ImGuiID DockspaceId)
{
    RenderToolbarPanel(0);
    SkeletonTreePanel.Render(EditingAsset, State, MakePanelName("Skeleton Tree").c_str(), 0);
    PreviewViewport.Render(EditingAsset, State, DeltaTime, MakePanelName("Preview Viewport").c_str(), 0);
    DetailsPanel.Render(EditingAsset, EditingAssetPath, State, MakePanelName("Details").c_str(), 0);
}

void FSkeletalMeshEditor::RenderToolbarPanel(ImGuiID DockspaceId)
{
    if (DockspaceId != 0)
    {
        ImGui::SetNextWindowDockID(DockspaceId, ImGuiCond_FirstUseEver);
    }

    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin(MakePanelName("Toolbar").c_str(), nullptr, Flags))
    {
        Toolbar.Render(EditingAsset, State);
    }
    ImGui::End();
}
