#pragma once

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <string>

/**
 * 여러 에디터에서 재사용할 수 있는 DockBuilder layout 유틸.
 *
 * 현재는 SkeletalMeshEditor의 기본 배치에 맞춘 4분할 레이아웃을 제공한다.
 * 이후 MaterialEditor / AnimationEditor 같은 에셋 에디터도 같은 형태를 재사용할 수 있다.
 */
struct FFourPanelDockLayoutDesc
{
    std::string ToolbarWindow;
    std::string LeftWindow;
    std::string CenterWindow;
    std::string RightWindow;

    float ToolbarRatio = 0.10f;
    float LeftRatio = 0.22f;
    float RightRatio = 0.28f;
};

class FEditorDockLayoutUtils
{
  public:
    static void DockFourPanelLayout(ImGuiID DockspaceId, const FFourPanelDockLayoutDesc &Desc)
    {
        if (DockspaceId == 0)
        {
            return;
        }

        // Level Editor 메인 DockSpace를 통째로 재생성하면 기존 레벨 패널이 날아간다.
        // 따라서 DockBuilderRemoveNode()는 호출하지 않고, 열린 에셋 에디터 패널만 현재 DockSpace에 추가한다.
        ImGuiID MainId = DockspaceId;
        ImGuiID ToolbarId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Up, Desc.ToolbarRatio, nullptr, &MainId);
        ImGuiID LeftId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Left, Desc.LeftRatio, nullptr, &MainId);
        ImGuiID RightId = ImGui::DockBuilderSplitNode(MainId, ImGuiDir_Right, Desc.RightRatio, nullptr, &MainId);
        ImGuiID CenterId = MainId;

        if (!Desc.ToolbarWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.ToolbarWindow.c_str(), ToolbarId);
        }
        if (!Desc.LeftWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.LeftWindow.c_str(), LeftId);
        }
        if (!Desc.CenterWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.CenterWindow.c_str(), CenterId);
        }
        if (!Desc.RightWindow.empty())
        {
            ImGui::DockBuilderDockWindow(Desc.RightWindow.c_str(), RightId);
        }

        ImGui::DockBuilderFinish(DockspaceId);
    }
};
