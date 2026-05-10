#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshPreviewViewport.h"

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

void FSkeletalMeshPreviewViewport::Render(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, float DeltaTime,
                                          const char *WindowName, ImGuiID DockspaceId)
{
    EnsureViewportResources();

    if (DockspaceId != 0)
    {
        ImGui::SetNextWindowDockID(DockspaceId, ImGuiCond_FirstUseEver);
    }

    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin(WindowName, nullptr, Flags))
    {
        RenderViewportPanel(Mesh, State, DeltaTime);
    }
    ImGui::End();
}

void FSkeletalMeshPreviewViewport::RenderViewportPanel(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, float DeltaTime)
{
    (void)DeltaTime;

    if (!PreviewViewportClient)
    {
        ImGui::TextDisabled("Preview viewport client is not initialized.");
        return;
    }

    PreviewViewportClient->SetPreviewMesh(Mesh);
    PreviewViewportClient->SetEditorState(&State);

    const ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
    ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
    if (CanvasSize.x < 1.0f)
    {
        CanvasSize.x = 1.0f;
    }
    if (CanvasSize.y < 1.0f)
    {
        CanvasSize.y = 1.0f;
    }

    FRect Rect;
    Rect.X = CanvasPos.x;
    Rect.Y = CanvasPos.y;
    Rect.Width = CanvasSize.x;
    Rect.Height = CanvasSize.y;

    PreviewViewportClient->SetHovered(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows));
    PreviewViewportClient->SetActive(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
    PreviewViewportClient->SetViewportScreenRect(Rect);
    PreviewViewportClient->RenderViewportImage(PreviewViewportClient->IsActive());

    // FEditorViewportClient 기반 Preview Viewport가 차지한 영역만큼 ImGui 레이아웃을 전진시킨다.
    ImGui::Dummy(CanvasSize);
}
