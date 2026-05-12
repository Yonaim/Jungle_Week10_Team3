#pragma once

#include "Core/CoreTypes.h"
#include "Core/PropertyTypes.h"
#include "ImGui/imgui.h"
#include "LevelEditor/UI/Panels/ContentBrowser/ContentItem.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Platform/Paths.h"
#include "Texture/Texture2D.h"

#include <string>
#include <type_traits>
#include <utility>

struct FContentItem;

namespace FEditorMaterialSlotUI
{
inline FString RemoveExtension(const FString &Path)
{
    const size_t DotPos = Path.find_last_of('.');
    return DotPos == FString::npos ? Path : Path.substr(0, DotPos);
}

inline FString GetStemFromPath(const FString &Path)
{
    const size_t SlashPos = Path.find_last_of("/\\");
    const FString FileName = SlashPos == FString::npos ? Path : Path.substr(SlashPos + 1);
    return RemoveExtension(FileName);
}

inline FString MakeAssetPreviewLabel(const FString &Path)
{
    if (Path.empty() || Path == "None")
    {
        return "None";
    }
    return GetStemFromPath(Path);
}

inline bool DrawMaterialSlotRow(FMaterialSlot &Slot, int32 ElementIndex, const FString &SlotName)
{
    bool bChanged = false;

    ImGui::BeginGroup();
    ImGui::Text("Element %d", ElementIndex);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::TextUnformatted(SlotName.empty() ? "None" : SlotName.c_str());
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", SlotName.empty() ? "None" : SlotName.c_str());
    }
    ImGui::EndGroup();

    ImGui::SameLine(120.0f);

    ImGui::BeginGroup();
    ImGui::PushID(ElementIndex);

    constexpr float PreviewImageSize = 26.0f;
    constexpr float PreviewSpacing = 6.0f;

    UMaterial *CurrentMaterial = (Slot.Path.empty() || Slot.Path == "None")
                                     ? nullptr
                                     : FMaterialManager::Get().GetOrCreateMaterial(Slot.Path);
    UTexture2D *CurrentPreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(CurrentMaterial);
    const float ReservedPreviewWidth =
        (CurrentPreviewTexture && CurrentPreviewTexture->GetSRV()) ? (PreviewImageSize + PreviewSpacing) : 0.0f;
    float ComboWidth = ImGui::GetContentRegionAvail().x - ReservedPreviewWidth;
    if (ComboWidth < 120.0f)
    {
        ComboWidth = 120.0f;
    }
    ImGui::SetNextItemWidth(ComboWidth);

    const FString Preview = MakeAssetPreviewLabel(Slot.Path);
    if (ImGui::BeginCombo("##MatCombo", Preview.c_str()))
    {
        const bool bSelectedNone = Slot.Path == "None" || Slot.Path.empty();
        if (ImGui::Selectable("None", bSelectedNone))
        {
            Slot.Path = "None";
            bChanged = true;
        }
        if (bSelectedNone)
        {
            ImGui::SetItemDefaultFocus();
        }

        const TArray<FMaterialAssetListItem> &MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
        for (const FMaterialAssetListItem &Item : MatFiles)
        {
            const bool bSelected = Slot.Path == Item.FullPath;
            UTexture2D *PreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(Item.FullPath);
            if (PreviewTexture && PreviewTexture->GetSRV())
            {
                ImGui::Image(PreviewTexture->GetSRV(), ImVec2(24.0f, 24.0f));
                ImGui::SameLine();
            }
            if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
            {
                Slot.Path = Item.FullPath;
                bChanged = true;
            }
            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (CurrentPreviewTexture && CurrentPreviewTexture->GetSRV())
    {
        ImGui::SameLine(0.0f, PreviewSpacing);
        ImGui::Image(CurrentPreviewTexture->GetSRV(), ImVec2(PreviewImageSize, PreviewImageSize));
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Preview.c_str());
        }
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *Payload = ImGui::AcceptDragDropPayload("MaterialContentItem"))
        {
            const FContentItem ContentItem = *reinterpret_cast<const FContentItem *>(Payload->Data);
            Slot.Path = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
            bChanged = true;
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
    ImGui::EndGroup();
    return bChanged;
}

namespace Private
{
template <typename T, typename = void>
struct THasMaterialSlotEditingApi : std::false_type
{
};

template <typename T>
struct THasMaterialSlotEditingApi<
    T, std::void_t<decltype(std::declval<T &>().EnsureMaterialSlotsForEditing()),
                   decltype(std::declval<T &>().GetMaterialSlotCount()),
                   decltype(std::declval<T &>().GetMaterialSlot(0))>> : std::true_type
{
};
} // namespace Private

template <typename TComponent>
inline bool RenderComponentOverrideMaterials(TComponent *Component, const char *EmptyMessage = "No material slots.")
{
    bool bChanged = false;
    if (!Component)
    {
        ImGui::TextDisabled("No preview component.");
        return false;
    }

    if constexpr (Private::THasMaterialSlotEditingApi<TComponent>::value)
    {
        Component->EnsureMaterialSlotsForEditing();
        const int32 SlotCount = Component->GetMaterialSlotCount();
        if (SlotCount <= 0)
        {
            ImGui::TextDisabled("%s", EmptyMessage);
            return false;
        }

        for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
        {
            if (FMaterialSlot *Slot = Component->GetMaterialSlot(SlotIndex))
            {
                FString SlotName = "Override Material";
                bChanged |= DrawMaterialSlotRow(*Slot, SlotIndex, SlotName);
            }
        }
    }
    else
    {
        ImGui::TextDisabled("Override material UI is waiting for component material-slot accessors.");
    }

    return bChanged;
}
} // namespace FEditorMaterialSlotUI
