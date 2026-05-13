#pragma once

#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Core/CoreTypes.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <string>
#include <vector>

/**
 * Panel 공통 descriptor.
 *
 * 역할:
 * - Level Editor 패널과 Asset Editor 패널이 같은 title/icon/inset 스타일을 쓰도록 한다.
 * - DisplayName은 사용자가 보는 이름이고, StableId는 ImGui 내부 ID 충돌 방지용 이름이다.
 * - 같은 이름의 Details / Viewport 패널이 여러 에디터에 생길 수 있으므로 StableId는 반드시 고유해야 한다.
 */
struct FPanelDesc
{
    const char *DisplayName = "";
    const char *StableId = "";
    const char *IconKey = nullptr;

    ImGuiID DockspaceId = 0;
    ImGuiCond DockCond = ImGuiCond_FirstUseEver;
    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoCollapse;

    bool bClosable = false;
    bool *bOpen = nullptr;

    bool bDrawTitleIcon = true;
    bool bApplyContentTopInset = true;
    bool bApplySideInset = true;
    bool bApplyBottomInset = true;
};

/**
 * Panel 공통 wrapper.
 *
 * 사용 이유:
 * - 기존 Level Editor 패널은 PanelTitleUtils 기반의 탭 스타일을 사용한다.
 * - SkeletalMeshEditor 같은 Asset Editor 패널도 이 wrapper를 통하면 같은 스타일을 유지할 수 있다.
 * - 패널마다 ImGui::Begin / DrawPanelTitleIcon / ApplyPanelContentTopInset 코드를 반복하지 않게 한다.
 */
class FPanel
{
  private:
    struct FBeginState
    {
        bool bDidBeginWindow = false;
        bool bPushedStyle = false;
        bool bPushedContentClip = false;
    };

    static std::vector<FBeginState> &GetBeginStateStack()
    {
        static std::vector<FBeginState> Stack;
        return Stack;
    }

  public:
    static std::string MakeTitle(const FPanelDesc &Desc)
    {
        const char *StableId = (Desc.StableId && Desc.StableId[0] != '\0') ? Desc.StableId : Desc.DisplayName;
        const bool bHasIcon = Desc.IconKey && Desc.IconKey[0] != '\0';
        const char *Prefix = bHasIcon ? "      " : "";
        return std::string(Prefix) + (Desc.DisplayName ? Desc.DisplayName : "") + "###" + StableId;
    }

    static bool Begin(const FPanelDesc &Desc)
    {
        FBeginState BeginState;

        // Window 메뉴에서 패널을 끈 상태라면 ImGui::Begin 자체를 호출하지 않는다.
        // 닫힌 p_open=false 상태로 Begin/End를 강제로 호출하면 docking 경로에서
        // style stack sanity check가 깨지거나 End mismatch가 발생할 수 있다.
        if (Desc.bClosable && Desc.bOpen && !*Desc.bOpen)
        {
            GetBeginStateStack().push_back(BeginState);
            return false;
        }

        if (Desc.DockspaceId != 0)
        {
            ImGui::SetNextWindowDockID(Desc.DockspaceId, Desc.DockCond);
        }

        // DockNode 오른쪽 끝의 전역 X / menu 버튼은 제거한다.
        // 각 패널 탭의 닫기 X는 ImGui::Begin(..., p_open, ...) 경로에 맡긴다.
        ImGuiWindowClass PanelWindowClass{};
        PanelWindowClass.DockNodeFlagsOverrideSet =
            ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;
        ImGui::SetNextWindowClass(&PanelWindowClass);

        bool bTempOpen = true;
        bool *OpenPtr = Desc.bClosable ? (Desc.bOpen ? Desc.bOpen : &bTempOpen) : nullptr;
        const std::string Title = MakeTitle(Desc);

        const bool bVisible = ImGui::Begin(Title.c_str(), OpenPtr, Desc.WindowFlags);
        BeginState.bDidBeginWindow = true;

        PanelTitleUtils::PushPanelStyle();
        BeginState.bPushedStyle = true;
        GetBeginStateStack().push_back(BeginState);

        const char *DecorationIconKey = (Desc.bDrawTitleIcon && Desc.IconKey) ? Desc.IconKey : nullptr;
        PanelTitleUtils::QueuePanelDecoration(DecorationIconKey, nullptr);

        if (bVisible)
        {
            if (Desc.bApplyContentTopInset)
            {
                PanelTitleUtils::ApplyPanelContentTopInset(Desc.bApplySideInset, Desc.bApplyBottomInset);

                // Dock 탭/고정 헤더 위쪽은 스크롤 가능한 body가 침범하면 안 된다.
                // 일부 패널이 스크롤 중 cursor/drawlist를 직접 조정하면 title/body 경계 위로 그려질 수 있으므로
                // 공통 Panel wrapper에서 content clip을 명시적으로 건다.
                ImGuiWindow *CurrentWindow = ImGui::GetCurrentWindow();
                if (CurrentWindow)
                {
                    const ImVec2 ClipMin(CurrentWindow->WorkRect.Min.x, CurrentWindow->DC.CursorPos.y);
                    const ImVec2 ClipMax(CurrentWindow->WorkRect.Max.x, CurrentWindow->WorkRect.Max.y);
                    ImGui::PushClipRect(ClipMin, ClipMax, true);
                    GetBeginStateStack().back().bPushedContentClip = true;
                }
            }
        }

        return bVisible;
    }

    static void End()
    {
        std::vector<FBeginState> &Stack = GetBeginStateStack();
        if (Stack.empty())
        {
            return;
        }

        const FBeginState BeginState = Stack.back();
        Stack.pop_back();

        if (!BeginState.bDidBeginWindow)
        {
            return;
        }

        if (BeginState.bPushedContentClip)
        {
            ImGui::PopClipRect();
        }
        if (BeginState.bPushedStyle)
        {
            PanelTitleUtils::PopPanelStyle();
        }
        ImGui::End();
    }
};
