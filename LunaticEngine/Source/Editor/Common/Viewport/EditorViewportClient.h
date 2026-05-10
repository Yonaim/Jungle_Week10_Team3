#pragma once

#include "Viewport/ViewportClient.h"
#include "UI/SWindow.h"

class FWindowsWindow;
class FViewport;

class FEditorViewportClient : public FViewportClient
{
  public:
    FEditorViewportClient() = default;
    ~FEditorViewportClient() override = default;

    virtual void Init(FWindowsWindow *InWindow);
    virtual void Shutdown();
    virtual void Tick(float DeltaTime);

    void SetViewport(FViewport *InViewport) { Viewport = InViewport; }
    FViewport *GetViewport() const { return Viewport; }

    void SetLayoutWindow(SWindow *InWindow) { LayoutWindow = InWindow; }
    SWindow *GetLayoutWindow() const { return LayoutWindow; }

    void SetActive(bool bInActive) { bIsActive = bInActive; }
    bool IsActive() const { return bIsActive; }

    void SetHovered(bool bInHovered) { bIsHovered = bInHovered; }
    bool IsHovered() const { return bIsHovered; }

    void SetViewportSize(float InWidth, float InHeight);

    const FRect &GetViewportScreenRect() const { return ViewportScreenRect; }

    virtual void UpdateLayoutRect();
    virtual void RenderViewportImage(bool bIsActiveViewport);

    bool GetCursorViewportPosition(uint32 &OutX, uint32 &OutY) const;

  protected:
    FWindowsWindow *Window = nullptr;
    FViewport *Viewport = nullptr;
    SWindow *LayoutWindow = nullptr;

    float WindowWidth = 1920.0f;
    float WindowHeight = 1080.0f;

    bool bIsActive = false;
    bool bIsHovered = false;

    FRect ViewportScreenRect;
};
