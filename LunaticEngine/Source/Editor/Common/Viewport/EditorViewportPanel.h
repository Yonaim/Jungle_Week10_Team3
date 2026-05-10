#pragma once

#include "Common/Viewport/EditorViewportClient.h"

#include "ImGui/imgui.h"

/**
 * ImGui 패널 안에 FEditorViewportClient를 배치하기 위한 공통 adapter.
 *
 * 역할:
 * - 현재 ImGui content region을 FRect로 계산한다.
 * - ViewportClient에 screen rect / size / focus 상태를 넘긴다.
 * - ViewportClient::RenderViewportImage() 호출 후 ImGui layout cursor를 viewport 크기만큼 전진시킨다.
 */
class FEditorViewportPanel
{
  public:
    static FRect CalculateContentRect()
    {
        const ImVec2 Pos = ImGui::GetCursorScreenPos();
        ImVec2 Size = ImGui::GetContentRegionAvail();
        if (Size.x < 1.0f)
        {
            Size.x = 1.0f;
        }
        if (Size.y < 1.0f)
        {
            Size.y = 1.0f;
        }

        FRect Rect;
        Rect.X = Pos.x;
        Rect.Y = Pos.y;
        Rect.Width = Size.x;
        Rect.Height = Size.y;
        return Rect;
    }

    static FRect RenderViewportClient(FEditorViewportClient &Client, bool bActive)
    {
        const FRect Rect = CalculateContentRect();
        Client.SetHovered(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows));
        Client.SetActive(bActive);
        Client.SetViewportScreenRect(Rect);
        Client.SetViewportSize(Rect.Width, Rect.Height);
        Client.RenderViewportImage(Client.IsActive());
        ImGui::Dummy(ImVec2(Rect.Width, Rect.Height));
        return Rect;
    }
};
