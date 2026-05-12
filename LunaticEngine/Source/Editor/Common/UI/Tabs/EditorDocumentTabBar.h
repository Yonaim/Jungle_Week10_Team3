#pragma once

#include "Common/UI/Panels/PanelTitleUtils.h"
#include "Core/CoreTypes.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace FEditorDocumentTabBar
{
struct FTabDesc
{
    std::string Label;
    std::string Tooltip;
    const char *IconKey = nullptr;
    ImVec4 IconTint = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    bool bDirty = false;
};

struct FStyle
{
    float Height = 34.0f;
    float OuterPaddingX = 8.0f;
    float TabGap = 2.0f;
    float TabMinWidth = 118.0f;
    float TabMaxWidth = 260.0f;
    float TabHeightPaddingY = 5.0f;
    float TabRounding = 6.0f;
    float HorizontalPadding = 13.0f;
    float CloseButtonSize = 16.0f;
    float IconSize = 18.0f;
    float IconTextGap = 8.0f;
    float TextSizeOffset = 1.0f;

    ImVec4 BarBg = ImVec4(0.035f, 0.035f, 0.040f, 1.0f);
    ImVec4 BarBottomLine = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
    ImVec4 InactiveTabBg = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    ImVec4 HoveredTabBg = ImVec4(0.15f, 0.15f, 0.16f, 0.85f);
    ImVec4 ActiveTabBg = ImVec4(0.14f, 0.14f, 0.15f, 1.0f);
    ImVec4 Text = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 CloseText = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
    ImVec4 CloseHoverBg = ImVec4(0.28f, 0.28f, 0.30f, 1.0f);
};

struct FRenderResult
{
    int32 SelectedIndex = -1;
    int32 CloseRequestedIndex = -1;
    bool bAnyHovered = false;
};

inline const FStyle &GetDefaultStyle()
{
    static const FStyle Style{};
    return Style;
}

inline float GetHeight()
{
    return GetDefaultStyle().Height;
}

inline void DrawWideTooltip(const FTabDesc &Tab)
{
    if (Tab.Label.empty() && Tab.Tooltip.empty())
    {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 0.0f), ImVec2(860.0f, FLT_MAX));
    if (ImGui::BeginTooltip())
    {
        ImGui::TextUnformatted(Tab.Label.c_str());
        if (!Tab.Tooltip.empty())
        {
            ImGui::Separator();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 780.0f);
            ImGui::TextUnformatted(Tab.Tooltip.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::EndTooltip();
    }
}

inline FRenderResult Render(const char *Id, const std::vector<FTabDesc> &Tabs, int32 ActiveTabIndex,
                            const FStyle &Style = GetDefaultStyle())
{
    FRenderResult Result;
    Result.SelectedIndex = ActiveTabIndex;

    if (Tabs.empty())
    {
        return Result;
    }

    const float BarHeight = ImGui::GetContentRegionAvail().y;
    const float BarWidth = ImGui::GetContentRegionAvail().x;
    if (BarWidth <= 32.0f || BarHeight <= 0.0f)
    {
        return Result;
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const ImVec2 BarMin = ImGui::GetCursorScreenPos();
    const ImVec2 BarMax(BarMin.x + BarWidth, BarMin.y + BarHeight);

    const int32 TabCount = static_cast<int32>(Tabs.size());
    const float TextRightPadding = 8.0f + Style.CloseButtonSize + 8.0f;
    const float AvailableWidth = (std::max)(0.0f, BarWidth - Style.TabGap * (std::max)(0, TabCount - 1));
    const float AutoWidth = TabCount > 0 ? AvailableWidth / static_cast<float>(TabCount) : AvailableWidth;
    const float TabWidth = (std::max)(Style.TabMinWidth, (std::min)(Style.TabMaxWidth, AutoWidth));
    const float TabHeight = (std::max)(1.0f, BarHeight - Style.TabHeightPaddingY);
    const float TabTop = BarMin.y + Style.TabHeightPaddingY;
    const float TextFontSize = ImGui::GetFontSize() + Style.TextSizeOffset;

    const ImU32 BarBgColor = ImGui::GetColorU32(Style.BarBg);
    const ImU32 BarBottomLineColor = ImGui::GetColorU32(Style.BarBottomLine);
    const ImU32 TabInactiveColor = ImGui::GetColorU32(Style.InactiveTabBg);
    const ImU32 TabHoverColor = ImGui::GetColorU32(Style.HoveredTabBg);
    const ImU32 TabActiveColor = ImGui::GetColorU32(Style.ActiveTabBg);
    const ImU32 TextColor = ImGui::GetColorU32(Style.Text);
    const ImU32 CloseTextColor = ImGui::GetColorU32(Style.CloseText);
    const ImU32 CloseHoverColor = ImGui::GetColorU32(Style.CloseHoverBg);

    DrawList->PushClipRect(BarMin, BarMax, true);
    DrawList->AddRectFilled(BarMin, BarMax, BarBgColor, 0.0f);
    DrawList->AddLine(ImVec2(BarMin.x, BarMax.y - 1.0f), ImVec2(BarMax.x, BarMax.y - 1.0f), BarBottomLineColor, 1.0f);

    float X = BarMin.x + Style.OuterPaddingX;
    ImGui::PushID(Id ? Id : "EditorDocumentTabBar");
    for (int32 Index = 0; Index < TabCount; ++Index)
    {
        const FTabDesc &Tab = Tabs[Index];

        if (X >= BarMax.x)
        {
            break;
        }

        const float CurrentTabWidth = (std::min)(TabWidth, BarMax.x - X - Style.OuterPaddingX);
        if (CurrentTabWidth <= 54.0f)
        {
            break;
        }

        std::string DisplayTitle = Tab.Label.empty() ? "Untitled" : Tab.Label;
        if (Tab.bDirty)
        {
            DisplayTitle += "*";
        }

        const ImVec2 TabMin(X, TabTop);
        const ImVec2 TabMax(X + CurrentTabWidth, TabTop + TabHeight);

        ImGui::PushID(Index);
        ImGui::SetCursorScreenPos(TabMin);
        ImGui::InvisibleButton("##DocumentTab", ImVec2(CurrentTabWidth, TabHeight));
        const bool bHovered = ImGui::IsItemHovered();
        Result.bAnyHovered |= bHovered;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            Result.SelectedIndex = Index;
        }

        const bool bSelected = Index == ActiveTabIndex;
        const ImU32 FillColor = bSelected ? TabActiveColor : (bHovered ? TabHoverColor : TabInactiveColor);
        if (bSelected || bHovered)
        {
            DrawList->AddRectFilled(TabMin, TabMax, FillColor, Style.TabRounding, ImDrawFlags_RoundCornersAll);
        }

        float TextX = TabMin.x + Style.HorizontalPadding;
        if (ID3D11ShaderResourceView *Icon = PanelTitleUtils::GetIcon(Tab.IconKey))
        {
            const float IconY = TabMin.y + (TabHeight - Style.IconSize) * 0.5f;
            const ImVec2 IconMin(TextX, IconY);
            DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), IconMin,
                               ImVec2(IconMin.x + Style.IconSize, IconMin.y + Style.IconSize), ImVec2(0.0f, 0.0f),
                               ImVec2(1.0f, 1.0f), ImGui::GetColorU32(Tab.IconTint));
            TextX += Style.IconSize + Style.IconTextGap;
        }

        const ImVec2 TextSize = ImGui::CalcTextSize(DisplayTitle.c_str());
        const float TextY = TabMin.y + (TabHeight - TextFontSize) * 0.5f;
        const ImVec2 TextMin(TextX, TextY);
        const ImVec2 TextClipMax(TabMax.x - TextRightPadding, TabMax.y);
        DrawList->PushClipRect(TextMin, TextClipMax, true);
        DrawList->AddText(ImGui::GetFont(), TextFontSize, TextMin, TextColor, DisplayTitle.c_str());
        DrawList->PopClipRect();

        const ImVec2 CloseMin(TabMax.x - Style.CloseButtonSize - 8.0f,
                              TabMin.y + (TabHeight - Style.CloseButtonSize) * 0.5f);
        ImGui::SetCursorScreenPos(CloseMin);
        ImGui::InvisibleButton("##CloseDocumentTab", ImVec2(Style.CloseButtonSize, Style.CloseButtonSize));
        const bool bCloseHovered = ImGui::IsItemHovered();
        if (bCloseHovered)
        {
            DrawList->AddRectFilled(CloseMin,
                                    ImVec2(CloseMin.x + Style.CloseButtonSize, CloseMin.y + Style.CloseButtonSize),
                                    CloseHoverColor, 3.0f);
        }
        DrawList->AddText(ImVec2(CloseMin.x + 4.0f, CloseMin.y - 1.0f), CloseTextColor, "x");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            Result.CloseRequestedIndex = Index;
        }

        if (bHovered && !bCloseHovered)
        {
            DrawWideTooltip(Tab);
        }

        ImGui::PopID();
        X += CurrentTabWidth + Style.TabGap;
    }
    ImGui::PopID();

    DrawList->PopClipRect();
    return Result;
}
} // namespace FEditorDocumentTabBar
