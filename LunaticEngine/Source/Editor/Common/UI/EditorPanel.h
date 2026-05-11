#pragma once

#include "Common/UI/EditorPanelTitleUtils.h"
#include "Core/CoreTypes.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <string>

/**
 * Editor panel 공통 descriptor.
 *
 * 역할:
 * - Level Editor 패널과 Asset Editor 패널이 같은 title/icon/inset 스타일을 쓰도록 한다.
 * - DisplayName은 사용자가 보는 이름이고, StableId는 ImGui 내부 ID 충돌 방지용 이름이다.
 * - 같은 이름의 Details / Viewport 패널이 여러 에디터에 생길 수 있으므로 StableId는 반드시 고유해야 한다.
 */
struct FEditorPanelDesc
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
 * Editor panel 공통 wrapper.
 *
 * 사용 이유:
 * - 기존 Level Editor 패널은 EditorPanelTitleUtils 기반의 탭 스타일을 사용한다.
 * - SkeletalMeshEditor 같은 Asset Editor 패널도 이 wrapper를 통하면 같은 스타일을 유지할 수 있다.
 * - 패널마다 ImGui::Begin / DrawPanelTitleIcon / ApplyPanelContentTopInset 코드를 반복하지 않게 한다.
 */
class FEditorPanel
{
  public:
    static std::string MakeTitle(const FEditorPanelDesc &Desc)
    {
        const char *StableId = (Desc.StableId && Desc.StableId[0] != '\0') ? Desc.StableId : Desc.DisplayName;
        const bool bHasIcon = Desc.IconKey && EditorPanelTitleUtils::GetEditorIcon(Desc.IconKey) != nullptr;
        const char *Prefix = bHasIcon ? "     " : "";
        return std::string(Prefix) + (Desc.DisplayName ? Desc.DisplayName : "") + "          ###" + StableId;
    }

    static bool Begin(const FEditorPanelDesc &Desc)
    {
        if (Desc.DockspaceId != 0)
        {
            ImGui::SetNextWindowDockID(Desc.DockspaceId, Desc.DockCond);
        }

        bool bTempOpen = true;
        bool *OpenPtr = Desc.bClosable ? (Desc.bOpen ? Desc.bOpen : &bTempOpen) : nullptr;
        const std::string Title = MakeTitle(Desc);
        const bool bVisible = ImGui::Begin(Title.c_str(), OpenPtr, Desc.WindowFlags);

        if (bVisible)
        {
            // 모든 공통 패널은 decoration queue에 등록한다.
            // 이유:
            // - EditorPanelTitleUtils::FlushPanelDecorations()가 dock tab bar의 빈 영역 fill과
            //   선택된 탭의 accent line을 그리기 위해 현재 패널의 DockNode/Tab 정보를 필요로 한다.
            // - IconKey가 없는 패널(예: Preview Viewport)도 기존 Level Editor 패널과 같은 tab chrome을 써야 한다.
            // - bOpen이 없는 임시 OpenPtr은 Flush 시점에 수명이 끝나므로 close button용 VisibleFlag로 넘기지 않는다.
            const char *DecorationIconKey = (Desc.bDrawTitleIcon && Desc.IconKey) ? Desc.IconKey : nullptr;
            bool *DecorationVisibleFlag = (Desc.bClosable && Desc.bOpen) ? Desc.bOpen : nullptr;
            EditorPanelTitleUtils::QueuePanelDecoration(DecorationIconKey, DecorationVisibleFlag);

            if (Desc.bApplyContentTopInset)
            {
                EditorPanelTitleUtils::ApplyPanelContentTopInset(Desc.bApplySideInset, Desc.bApplyBottomInset);
            }
        }

        return bVisible;
    }

    static void End()
    {
        ImGui::End();
    }
};
