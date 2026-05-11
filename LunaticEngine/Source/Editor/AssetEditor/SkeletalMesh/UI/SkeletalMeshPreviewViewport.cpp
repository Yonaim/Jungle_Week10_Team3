#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshPreviewViewport.h"

#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"
#include "Common/UI/EditorPanel.h"
#include "Common/UI/EditorViewportToolbar.h"
#include "Common/Viewport/EditorViewportPanel.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "Render/Pipeline/Renderer.h"
#include "UI/SWindow.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

FSkeletalMeshPreviewViewport::~FSkeletalMeshPreviewViewport()
{
    Shutdown();
}

void FSkeletalMeshPreviewViewport::Initialize(FWindowsWindow *InWindow, FRenderer *InRenderer)
{
    Window = InWindow;
    Renderer = InRenderer;
    EnsureViewportResources();
}

void FSkeletalMeshPreviewViewport::Shutdown()
{
    if (PreviewViewportClient)
    {
        PreviewViewportClient->Shutdown();
    }

    PreviewViewport.reset();
    PreviewViewportClient.reset();
    PreviewLayoutWindow.reset();

    Renderer = nullptr;
    Window = nullptr;
}

void FSkeletalMeshPreviewViewport::Tick(float DeltaTime)
{
    if (PreviewViewportClient)
    {
        PreviewViewportClient->Tick(DeltaTime);
    }
}

void FSkeletalMeshPreviewViewport::EnsureViewportResources()
{
    if (!Renderer || PreviewViewportClient)
    {
        return;
    }

    PreviewViewportClient = std::make_unique<FSkeletalMeshPreviewViewportClient>();
    PreviewViewportClient->Init(Window);

    PreviewViewport = std::make_unique<FViewport>();
    PreviewViewport->Initialize(Renderer->GetFD3DDevice().GetDevice(), 512, 512);
    PreviewViewport->SetClient(PreviewViewportClient.get());
    PreviewViewportClient->SetViewport(PreviewViewport.get());

    PreviewLayoutWindow = std::make_unique<SWindow>();
    PreviewViewportClient->SetLayoutWindow(PreviewLayoutWindow.get());
}

void FSkeletalMeshPreviewViewport::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, FSkeletalMeshEditorToolbar *Toolbar,
                                          float DeltaTime, const FEditorPanelDesc &PanelDesc)
{
    EnsureViewportResources();

    if (FEditorPanel::Begin(PanelDesc))
    {
        RenderViewportPanel(Mesh, State, Toolbar, DeltaTime);
    }
    FEditorPanel::End();
}

void FSkeletalMeshPreviewViewport::RenderViewportPanel(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State,
                                                       FSkeletalMeshEditorToolbar *Toolbar, float DeltaTime)
{
    (void)DeltaTime;

    if (!PreviewViewportClient)
    {
        ImGui::TextDisabled("Preview viewport client is not initialized.");
        return;
    }

    PreviewViewportClient->SetPreviewMesh(Mesh);
    PreviewViewportClient->SetEditorState(&State);

    if (Toolbar && FEditorViewportToolbar::Begin("##SkeletalMeshViewportToolbar"))
    {
        Toolbar->RenderViewportToolbar(Mesh, State);
    }
    if (Toolbar)
    {
        FEditorViewportToolbar::End();
    }

    const bool bActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    FEditorViewportPanel::RenderViewportClient(*PreviewViewportClient, bActive);
}
