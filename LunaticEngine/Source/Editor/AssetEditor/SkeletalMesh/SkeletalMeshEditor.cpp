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
    bOpen = true;

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
    bOpen = false;
    bCapturingInput = false;
}

bool FSkeletalMeshEditor::Save()
{
    FNotificationManager::Get().AddNotification("SkeletalMeshEditor save is not implemented yet.",
                                                ENotificationType::Info, 3.0f);
    return false;
}

void FSkeletalMeshEditor::RenderContent(float DeltaTime)
{
    (void)DeltaTime;
    if (!bOpen)
    {
        bCapturingInput = false;
        return;
    }

    bool bWindowOpen = bOpen;
    ImGui::SetNextWindowSize(ImVec2(720.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Skeletal Mesh Editor", &bWindowOpen, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        bOpen = bWindowOpen;
        bCapturingInput = bOpen;
        return;
    }

    bOpen = bWindowOpen;
    bCapturingInput = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
                      ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    const FString AssetName =
        EditingAssetPath.empty() ? FString("Untitled") : FPaths::ToUtf8(EditingAssetPath.filename().wstring());
    ImGui::Text("Asset: %s", AssetName.c_str());
    ImGui::Separator();
    ImGui::TextDisabled("SkeletalMeshEditor stub is connected.");
    if (EditingAsset)
    {
        ImGui::Text("Bones: %d", EditingAsset->GetBoneCount());
        ImGui::Text("Vertices: %d", EditingAsset->GetVertexCount());
        ImGui::Text("Indices: %d", EditingAsset->GetIndexCount());
    }

    ImGui::End();
}
