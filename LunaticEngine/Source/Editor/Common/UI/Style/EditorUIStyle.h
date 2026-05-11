#pragma once

#include "Common/UI/Style/AccentColor.h"
#include "Core/CoreTypes.h"
#include "Object/FName.h"
#include "Resource/ResourceManager.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <string>

struct ID3D11ShaderResourceView;

// 에디터 전역에서 공유하는 ImGui 스타일/위젯 헬퍼 모음.
// 레벨 에디터 패널에만 하드코딩되어 있던 Details, Outliner, Viewport Toolbar 스타일을
// 에셋 에디터에서도 같은 형태로 재사용하기 위해 이 파일에 모은다.
namespace FEditorUIStyle
{
inline constexpr ImVec4 PopupMenuItemColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
inline constexpr ImVec4 PopupMenuItemHoverColor = UIAccentColor::Value;
inline constexpr ImVec4 PopupMenuItemActiveColor = UIAccentColor::Value;
inline constexpr ImVec4 PopupSectionHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);

inline constexpr ImVec4 HeaderButtonColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
inline constexpr ImVec4 HeaderButtonHoveredColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
inline constexpr ImVec4 HeaderButtonActiveColor = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
inline constexpr ImVec4 HeaderButtonBorderColor = ImVec4(0.42f, 0.42f, 0.45f, 0.90f);

inline constexpr ImVec4 SurfaceBg = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
inline constexpr ImVec4 PopupBg = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
inline constexpr ImVec4 FieldBg = ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f);
inline constexpr ImVec4 FieldHoverBg = ImVec4(33.0f / 255.0f, 33.0f / 255.0f, 33.0f / 255.0f, 1.0f);
inline constexpr ImVec4 FieldActiveBg = ImVec4(43.0f / 255.0f, 43.0f / 255.0f, 43.0f / 255.0f, 1.0f);
inline constexpr ImVec4 FieldBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);

inline constexpr ImVec4 DetailsSectionTextColor = ImVec4(0.76f, 0.76f, 0.78f, 1.0f);
inline constexpr ImVec4 DetailsSectionHeaderColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
inline constexpr ImVec4 DetailsSectionHeaderHoveredColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
inline constexpr ImVec4 DetailsSectionHeaderActiveColor = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);

inline constexpr ImVec4 DetailsVectorLabelColor = ImVec4(0.83f, 0.84f, 0.87f, 1.0f);
inline constexpr ImVec4 DetailsVectorFieldBg = ImVec4(10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.0f);
inline constexpr ImVec4 DetailsVectorFieldHoverBg = ImVec4(15.0f / 255.0f, 15.0f / 255.0f, 15.0f / 255.0f, 1.0f);
inline constexpr ImVec4 DetailsVectorFieldActiveBg = ImVec4(20.0f / 255.0f, 20.0f / 255.0f, 20.0f / 255.0f, 1.0f);
inline constexpr ImVec4 DetailsVectorResetButtonColor = ImVec4(0.22f, 0.22f, 0.23f, 1.0f);
inline constexpr ImVec4 DetailsVectorResetButtonHoveredColor = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
inline constexpr ImVec4 DetailsVectorResetButtonActiveColor = ImVec4(0.36f, 0.36f, 0.38f, 1.0f);
inline constexpr ImVec4 DetailsVectorResetButtonBorderColor = ImVec4(0.52f, 0.52f, 0.55f, 0.95f);
inline constexpr float DetailsLabelWidth = 124.0f;
inline constexpr float DetailsPropertyVerticalSpacing = 6.0f;
inline constexpr float DetailsVectorResetSpacing = 6.0f;

inline constexpr ImVec4 OutlinerSelectionHeaderColor = UIAccentColor::Value;
inline constexpr ImVec4 OutlinerSelectionHeaderHoveredColor = UIAccentColor::Value;
inline constexpr ImVec4 OutlinerSelectionHeaderActiveColor = UIAccentColor::Value;
inline constexpr ImVec4 OutlinerFolderArrowColor = ImVec4(0.66f, 0.66f, 0.68f, 1.0f);
inline constexpr ImVec4 OutlinerItemLabelColor = ImVec4(0.86f, 0.86f, 0.88f, 1.0f);
inline constexpr ImU32 OutlinerFolderIconTint = IM_COL32(184, 140, 58, 255);

inline ID3D11ShaderResourceView *GetIcon(const char *Key)
{
    return FResourceManager::Get().FindLoadedTexture(FResourceManager::Get().ResolvePath(FName(Key))).Get();
}

inline void DrawPopupSectionHeader(const char *Label)
{
    ImGui::PushStyleColor(ImGuiCol_Text, PopupSectionHeaderTextColor);
    ImGui::SeparatorText(Label);
    ImGui::PopStyleColor();
}

inline void PushHeaderButtonStyle(float FrameRounding = 6.0f)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, FrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, HeaderButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HeaderButtonHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HeaderButtonActiveColor);
    ImGui::PushStyleColor(ImGuiCol_Border, HeaderButtonBorderColor);
}

inline void PopHeaderButtonStyle()
{
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

inline void PushPopupMenuStyle()
{
    ImGui::PushStyleColor(ImGuiCol_PopupBg, PopupBg);
    ImGui::PushStyleColor(ImGuiCol_Header, PopupMenuItemColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, PopupMenuItemHoverColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, PopupMenuItemActiveColor);
}

inline void PopPopupMenuStyle()
{
    ImGui::PopStyleColor(4);
}

inline bool DrawSearchInputWithIcon(const char *Id, const char *Hint, char *Buffer, size_t BufferSize, float Width)
{
    ImGuiStyle &Style = ImGui::GetStyle();
    ImGui::SetNextItemWidth(Width);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 11.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Style.FramePadding.x + 26.0f, Style.FramePadding.y));
    ImGui::PushStyleColor(ImGuiCol_Border, HeaderButtonBorderColor);
    const std::string PaddedHint = std::string("   ") + Hint;
    const bool bChanged = ImGui::InputTextWithHint(Id, PaddedHint.c_str(), Buffer, BufferSize);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    const ImVec2 Min = ImGui::GetItemRectMin();
    const ImVec2 Max = ImGui::GetItemRectMax();
    const float LeadingSlotWidth = (std::min)(30.0f, Max.x - Min.x);
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(Min.x + 1.0f, Min.y + 1.0f),
                                              ImVec2(Min.x + LeadingSlotWidth, Max.y - 1.0f), IM_COL32(5, 5, 5, 255),
                                              11.0f, ImDrawFlags_RoundCornersLeft);

    if (ID3D11ShaderResourceView *SearchIcon = GetIcon("Editor.Icon.Search"))
    {
        const float IconSize = ImGui::GetFrameHeight() - 12.0f;
        const float IconY = Min.y + (ImGui::GetFrameHeight() - IconSize) * 0.5f;
        ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(SearchIcon), ImVec2(Min.x + 7.0f, IconY),
                                             ImVec2(Min.x + 7.0f + IconSize, IconY + IconSize), ImVec2(1.0f, 0.0f),
                                             ImVec2(0.0f, 1.0f), IM_COL32(210, 210, 210, 255));
    }

    return bChanged;
}

inline bool BeginDetailsSection(const char *SectionName)
{
    const std::string HeaderId = std::string(SectionName) + "##DetailsSection";
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, DetailsSectionTextColor);
    ImGui::PushStyleColor(ImGuiCol_Header, DetailsSectionHeaderColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, DetailsSectionHeaderHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, DetailsSectionHeaderActiveColor);
    const bool bOpen = ImGui::CollapsingHeader(HeaderId.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    return bOpen;
}

inline void PushDetailsFieldStyle()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, FieldBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, FieldHoverBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, FieldActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Border, FieldBorder);
}

inline void PopDetailsFieldStyle()
{
    ImGui::PopStyleColor(4);
}

inline void PushDetailsVectorFieldStyle()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, DetailsVectorFieldBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, DetailsVectorFieldHoverBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, DetailsVectorFieldActiveBg);
    ImGui::PushStyleColor(ImGuiCol_Border, FieldBorder);
}

inline void PopDetailsVectorFieldStyle()
{
    ImGui::PopStyleColor(4);
}

inline void PushDetailsVectorResetButtonStyle()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, DetailsVectorResetButtonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DetailsVectorResetButtonHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, DetailsVectorResetButtonActiveColor);
    ImGui::PushStyleColor(ImGuiCol_Border, DetailsVectorResetButtonBorderColor);
}

inline void PopDetailsVectorResetButtonStyle()
{
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}

inline void PushDetailsPropertyEditStyle()
{
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
}

inline void PopDetailsPropertyEditStyle()
{
    ImGui::PopStyleColor(7);
}

inline bool BeginDetailsReadOnlyTable(const char *Id, float LabelColumnWidth = DetailsLabelWidth)
{
    if (!ImGui::BeginTable(Id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
    {
        return false;
    }
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, LabelColumnWidth);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

inline void DrawReadOnlyTextRow(const char *Label, const char *Value)
{
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(Label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(Value ? Value : "");
}

inline void DrawReadOnlyIntRow(const char *Label, int32 Value)
{
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(Label);
    ImGui::TableNextColumn();
    ImGui::Text("%d", Value);
}

inline bool BeginOutlinerTable(const char *Id)
{
    return ImGui::BeginTable(Id, 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp);
}

inline void SetupOutlinerTableColumns()
{
    ImGui::TableSetupColumn("##Visibility", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 28.0f);
    ImGui::TableSetupColumn("##Lock", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 28.0f);
    ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch, 260.0f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableHeadersRow();
}

inline void PushOutlinerSelectionRowStyle()
{
    ImGui::PushStyleColor(ImGuiCol_Text, OutlinerItemLabelColor);
    ImGui::PushStyleColor(ImGuiCol_Header, OutlinerSelectionHeaderColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, OutlinerSelectionHeaderHoveredColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, OutlinerSelectionHeaderActiveColor);
}

inline void PopOutlinerSelectionRowStyle()
{
    ImGui::PopStyleColor(4);
}
} // namespace FEditorUIStyle
