#include "Common/Viewport/EditorViewportClient.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

namespace
{
bool TryConvertMouseToViewportPixel(const ImVec2 &MousePos, const FRect &ViewportScreenRect, const FViewport *Viewport,
                                    float FallbackWidth, float FallbackHeight, float &OutViewportX,
                                    float &OutViewportY)
{
    if (ViewportScreenRect.Width <= 0.0f || ViewportScreenRect.Height <= 0.0f)
    {
        return false;
    }

    const float LocalX = MousePos.x - ViewportScreenRect.X;
    const float LocalY = MousePos.y - ViewportScreenRect.Y;
    if (LocalX < 0.0f || LocalY < 0.0f || LocalX >= ViewportScreenRect.Width || LocalY >= ViewportScreenRect.Height)
    {
        return false;
    }

    const float TargetWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : FallbackWidth;
    const float TargetHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : FallbackHeight;
    if (TargetWidth <= 0.0f || TargetHeight <= 0.0f)
    {
        return false;
    }

    const float ScaleX = TargetWidth / ViewportScreenRect.Width;
    const float ScaleY = TargetHeight / ViewportScreenRect.Height;
    OutViewportX = LocalX * ScaleX;
    OutViewportY = LocalY * ScaleY;
    return true;
}
} // namespace

void FEditorViewportClient::Init(FWindowsWindow *InWindow)
{
    Window = InWindow;
}

void FEditorViewportClient::Shutdown()
{
    Viewport = nullptr;
    LayoutWindow = nullptr;
    Window = nullptr;
}

void FEditorViewportClient::Tick(float DeltaTime)
{
    (void)DeltaTime;
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
    if (InWidth > 0.0f)
    {
        WindowWidth = InWidth;
    }
    if (InHeight > 0.0f)
    {
        WindowHeight = InHeight;
    }
}

void FEditorViewportClient::UpdateLayoutRect()
{
    if (!LayoutWindow)
    {
        return;
    }

    const FRect &R = LayoutWindow->GetRect();
    ViewportScreenRect = R;
    SetViewportSize(R.Width, R.Height);

    if (!Viewport)
    {
        return;
    }

    const uint32 SlotW = static_cast<uint32>(R.Width);
    const uint32 SlotH = static_cast<uint32>(R.Height);
    if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
    {
        Viewport->RequestResize(SlotW, SlotH);
    }
}

void FEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
    if (!Viewport || !Viewport->GetSRV())
    {
        return;
    }

    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0 || R.Height <= 0)
    {
        return;
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 Min(R.X, R.Y);
    const ImVec2 Max(R.X + R.Width, R.Y + R.Height);
    DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);

    if (!bIsActiveViewport)
    {
        return;
    }

    constexpr float ActiveBorderThickness = 4.0f;
    const float BorderInset = ActiveBorderThickness * 0.5f;
    if (R.Width > ActiveBorderThickness && R.Height > ActiveBorderThickness)
    {
        DrawList->AddRect(ImVec2(Min.x + BorderInset, Min.y + BorderInset),
                          ImVec2(Max.x - BorderInset, Max.y - BorderInset), IM_COL32(255, 165, 0, 220), 0.0f, 0,
                          ActiveBorderThickness);
    }
}

bool FEditorViewportClient::GetCursorViewportPosition(uint32 &OutX, uint32 &OutY) const
{
    const ImVec2 MousePos = ImGui::GetIO().MousePos;
    if (!bIsHovered && !bIsActive)
    {
        return false;
    }

    float ViewportX = 0.0f;
    float ViewportY = 0.0f;
    if (!TryConvertMouseToViewportPixel(MousePos, ViewportScreenRect, Viewport, WindowWidth, WindowHeight, ViewportX,
                                        ViewportY))
    {
        return false;
    }

    OutX = static_cast<uint32>(ViewportX);
    OutY = static_cast<uint32>(ViewportY);
    return true;
}
