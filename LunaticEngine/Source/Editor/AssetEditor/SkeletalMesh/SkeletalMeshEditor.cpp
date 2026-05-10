#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"

#include "Core/Notification.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

#include "ImGui/imgui.h"

void FSkeletalMeshEditor::Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;
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
    (void)DeltaTime;
}

void FSkeletalMeshEditor::RenderContent(float DeltaTime)
{
    if (!bOpen)
    {
        bCapturingInput = false;
        return;
    }

    bCapturingInput = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
                      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    Toolbar.Render(EditingAsset, State);
    ImGui::Separator();
    RenderLayout(DeltaTime);
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

void FSkeletalMeshEditor::RenderLayout(float DeltaTime)
{
    if (ImGui::BeginTable("##SkeletalMeshEditorLayout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Skeleton", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 360.0f);

        ImGui::TableNextColumn();
        SkeletonTreePanel.Render(EditingAsset, State);

        ImGui::TableNextColumn();
        PreviewViewport.Render(EditingAsset, State, DeltaTime);

        ImGui::TableNextColumn();
        DetailsPanel.Render(EditingAsset, EditingAssetPath, State);

        ImGui::EndTable();
    }
}
