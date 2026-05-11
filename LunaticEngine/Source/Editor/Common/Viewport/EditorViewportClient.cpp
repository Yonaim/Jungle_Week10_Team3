#include "Common/Viewport/EditorViewportClient.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"

#include <algorithm>

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
    bool bSizeChanged = false;

    if (InWidth > 0.0f && WindowWidth != InWidth)
    {
        WindowWidth = InWidth;
        bSizeChanged = true;
    }
    if (InHeight > 0.0f && WindowHeight != InHeight)
    {
        WindowHeight = InHeight;
        bSizeChanged = true;
    }

    // Editor viewport camera is not a UCameraComponent anymore, so resize notifications
    // are not delivered through component/World paths. Keep the shared viewport camera
    // aspect ratio synchronized with the actual ImGui viewport rect here.
    //
    // Without this, Asset Preview viewports can render helper scene data such as grid/gizmo
    // against the current render target size while the mesh camera still keeps the old
    // aspect ratio, which makes the scene range and mesh range look different.
    if (bSizeChanged && WindowWidth > 0.0f && WindowHeight > 0.0f)
    {
        ViewCamera.OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
    }
}


void FEditorViewportClient::SetViewportScreenRect(const FRect &InRect)
{
    ViewportScreenRect = InRect;
    SetViewportSize(InRect.Width, InRect.Height);

    if (!Viewport)
    {
        return;
    }

    const uint32 NewWidth = static_cast<uint32>(InRect.Width > 0.0f ? InRect.Width : 0.0f);
    const uint32 NewHeight = static_cast<uint32>(InRect.Height > 0.0f ? InRect.Height : 0.0f);
    if (NewWidth > 0 && NewHeight > 0 && (NewWidth != Viewport->GetWidth() || NewHeight != Viewport->GetHeight()))
    {
        Viewport->RequestResize(NewWidth, NewHeight);
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

void FEditorViewportClient::RenderViewportTooltipBar() const
{
    const char *TooltipBarText = GetViewportTooltipBarText();
    if (!TooltipBarText || TooltipBarText[0] == '\0')
    {
        return;
    }

    const FRect &R = ViewportScreenRect;
    if (R.Width <= 0.0f || R.Height <= 0.0f)
    {
        return;
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 TextSize = ImGui::CalcTextSize(TooltipBarText);
    const ImVec2 Padding(12.0f, 7.0f);
    const float  Margin = 12.0f;
    const float  BarHeight = TextSize.y + Padding.y * 2.0f;
    const float  MaxBarWidth = (std::max)(120.0f, R.Width - Margin * 2.0f);
    const float  DesiredBarWidth = TextSize.x + Padding.x * 2.0f;
    const float  BarWidth = (std::min)(MaxBarWidth, DesiredBarWidth);

    const ImVec2 BarMin(R.X + Margin, R.Y + R.Height - Margin - BarHeight);
    const ImVec2 BarMax(BarMin.x + BarWidth, BarMin.y + BarHeight);
    const ImVec2 TextMin(BarMin.x + Padding.x, BarMin.y + Padding.y);

    DrawList->AddRectFilled(BarMin, BarMax, IM_COL32(20, 22, 26, 210), 8.0f);
    DrawList->AddRect(BarMin, BarMax, IM_COL32(70, 74, 82, 220), 8.0f);
    DrawList->PushClipRect(BarMin, BarMax, true);
    DrawList->AddText(TextMin, IM_COL32(190, 196, 205, 255), TooltipBarText);
    DrawList->PopClipRect();
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
