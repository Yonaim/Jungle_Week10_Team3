#pragma once

#include "Viewport/ViewportClient.h"
#include "UI/SWindow.h"

class FWindowsWindow;
class FViewport;

/**
 * 모든 Editor Viewport Client의 공통 베이스.
 *
 * 역할:
 * - FViewport 포인터와 ImGui/SWindow layout 정보를 보관한다.
 * - Viewport screen rect, active/hovered 상태, viewport image 렌더링 같은 공통 기능을 제공한다.
 *
 * 주의:
 * - 이 클래스는 Level Editor 전용 기능을 몰라야 한다.
 * - Actor picking, Gizmo, Selection, Camera navigation, PIE shortcut은 FLevelEditorViewportClient 쪽에 둔다.
 * - SkeletalMeshPreviewViewportClient 같은 Asset Preview 뷰포트도 이 베이스를 상속한다.
 */
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
    void SetViewportScreenRect(const FRect &InRect);

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
