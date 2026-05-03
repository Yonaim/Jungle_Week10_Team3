#include "Editor/UI/EditorPropertyWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/UI/EditorPanelTitleUtils.h"

#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/DecalComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/HeightFogComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Component/MeshComponent.h"
#include "Component/Movement/MovementComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TextRenderComponent.h"
#include "Core/ClassTypes.h"
#include "Core/PropertyTypes.h"
#include "GameFramework/World.h"
#include "ImGui/imgui.h"
#include "Materials/Material.h"
#include "Mesh/ObjManager.h"
#include "Mesh/StaticMesh.h"
#include "Object/FName.h"
#include "Object/ObjectIterator.h"
#include "Platform/Paths.h"
#include "Resource/ResourceManager.h"
#include "Texture/Texture2D.h"


#include <Windows.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <commdlg.h>
#include <cstring>
#include <filesystem>
#include <string>


#include "Materials/MaterialManager.h"

#define SEPARATOR()                                                                                                                                  \
    ;                                                                                                                                                \
    ImGui::Spacing();                                                                                                                                \
    ImGui::Spacing();                                                                                                                                \
    ImGui::Separator();                                                                                                                              \
    ImGui::Spacing();                                                                                                                                \
    ImGui::Spacing();

namespace
{
    constexpr ImVec4 PopupMenuItemHoverColor = ImVec4(0.10f, 0.54f, 0.96f, 1.0f);
    constexpr ImVec4 PopupMenuItemActiveColor = ImVec4(0.00f, 0.40f, 0.84f, 1.0f);
    constexpr ImVec4 DetailsHeaderButtonColor = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    constexpr ImVec4 DetailsHeaderButtonHoveredColor = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
    constexpr ImVec4 DetailsHeaderButtonActiveColor = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    constexpr ImVec4 DetailsHeaderButtonBorderColor = ImVec4(0.42f, 0.42f, 0.45f, 0.90f);

    namespace PopupPalette
    {
        constexpr ImVec4 PopupBg = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
        constexpr ImVec4 SurfaceBg = ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f);
        constexpr ImVec4 FieldBg = ImVec4(26.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 1.0f);
        constexpr ImVec4 FieldHoverBg = ImVec4(33.0f / 255.0f, 33.0f / 255.0f, 33.0f / 255.0f, 1.0f);
        constexpr ImVec4 FieldActiveBg = ImVec4(43.0f / 255.0f, 43.0f / 255.0f, 43.0f / 255.0f, 1.0f);
        constexpr ImVec4 FieldBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
    } // namespace PopupPalette

    FString GetEditorPathResource(const char *Key) { return FResourceManager::Get().ResolvePath(FName(Key)); }

    ID3D11ShaderResourceView *GetEditorIcon(const char *Key) { return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource(Key)).Get(); }

    void PushDetailsHeaderButtonStyle(float FrameRounding = 6.0f)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, FrameRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, DetailsHeaderButtonColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DetailsHeaderButtonHoveredColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, DetailsHeaderButtonActiveColor);
        ImGui::PushStyleColor(ImGuiCol_Border, DetailsHeaderButtonBorderColor);
    }

    void PopDetailsHeaderButtonStyle()
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }

    const char *GetActorHeaderIconKey(const AActor *Actor)
    {
        if (!Actor)
        {
            return "Editor.Icon.Actor";
        }

        const FString ClassName = Actor->GetClass()->GetName();
        if (ClassName.find("Character") != FString::npos)
        {
            return "Editor.Icon.Character";
        }
        if (ClassName.find("Pawn") != FString::npos)
        {
            return "Editor.Icon.Pawn";
        }
        if (ClassName.find("SpotLight") != FString::npos)
        {
            return "Editor.Icon.SpotLight";
        }
        if (ClassName.find("PointLight") != FString::npos)
        {
            return "Editor.Icon.PointLight";
        }
        if (ClassName.find("DirectionalLight") != FString::npos)
        {
            return "Editor.Icon.DirectionalLight";
        }
        if (ClassName.find("AmbientLight") != FString::npos)
        {
            return "Editor.Icon.AmbientLight";
        }
        if (ClassName.find("Decal") != FString::npos)
        {
            return "Editor.Icon.Decal";
        }
        if (Actor->GetRootComponent() && Actor->GetRootComponent()->IsA<UStaticMeshComponent>())
        {
            return "Editor.Icon.StaticMeshActor";
        }
        return "Editor.Icon.Actor";
    }

    bool DrawAddHeaderButton(const char *Id, const ImVec2 &Size)
    {
        PushDetailsHeaderButtonStyle();
        const bool   bClicked = ImGui::Button(Id, Size);
        PopDetailsHeaderButtonStyle();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        ImDrawList  *DrawList = ImGui::GetWindowDrawList();
        const ImU32  PlusColor = IM_COL32(116, 201, 72, 255);
        const ImU32  TextColor = ImGui::GetColorU32(ImGuiCol_Text);
        const char  *PlusText = "+";
        const char  *LabelText = "Add";
        const ImVec2 PlusSize = ImGui::CalcTextSize(PlusText);
        const ImVec2 LabelSize = ImGui::CalcTextSize(LabelText);
        const float  TotalWidth = PlusSize.x + 5.0f + LabelSize.x;
        const float  StartX = Min.x + ((Max.x - Min.x) - TotalWidth) * 0.5f;
        const float  TextY = Min.y + ((Max.y - Min.y) - LabelSize.y) * 0.5f;
        DrawList->AddText(ImVec2(StartX, TextY), PlusColor, PlusText);
        DrawList->AddText(ImVec2(StartX + PlusSize.x + 5.0f, TextY), TextColor, LabelText);
        return bClicked;
    }

    bool DrawSearchInputWithIcon(const char *Id, const char *Hint, char *Buffer, size_t BufferSize, float Width)
    {
        ImGuiStyle &Style = ImGui::GetStyle();
        ImGui::SetNextItemWidth(Width);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 11.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Style.FramePadding.x + 26.0f, Style.FramePadding.y));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.42f, 0.45f, 0.90f));
        const std::string PaddedHint = std::string("   ") + Hint;
        const bool        bChanged = ImGui::InputTextWithHint(Id, PaddedHint.c_str(), Buffer, BufferSize);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        if (ID3D11ShaderResourceView *SearchIcon = GetEditorIcon("Editor.Icon.Search"))
        {
            const ImVec2 Min = ImGui::GetItemRectMin();
            const float  IconSize = ImGui::GetFrameHeight() - 12.0f;
            const float  IconY = Min.y + (ImGui::GetFrameHeight() - IconSize) * 0.5f;
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(SearchIcon), ImVec2(Min.x + 7.0f, IconY),
                                                 ImVec2(Min.x + 7.0f + IconSize, IconY + IconSize), ImVec2(1.0f, 0.0f), ImVec2(0.0f, 1.0f),
                                                 IM_COL32(210, 210, 210, 255));
        }

        return bChanged;
    }

    bool DrawHeaderIconButton(const char *Id, const char *IconKey, const char *FallbackLabel, const char *Tooltip, const ImVec2 &Size,
                              ImU32 Tint = IM_COL32_WHITE)
    {
        PushDetailsHeaderButtonStyle();
        const bool   bClicked = ImGui::Button(Id, Size);
        PopDetailsHeaderButtonStyle();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        ImDrawList  *DrawList = ImGui::GetWindowDrawList();

        if (ID3D11ShaderResourceView *Icon = GetEditorIcon(IconKey))
        {
            const ImVec2 ItemSize = ImGui::GetItemRectSize();
            const float  IconSize = (std::min)(ItemSize.x, ItemSize.y) - 8.0f;
            const float  IconX = Min.x + ((Max.x - Min.x) - IconSize) * 0.5f;
            const float  IconY = Min.y + ((Max.y - Min.y) - IconSize) * 0.5f;
            DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), ImVec2(IconX, IconY), ImVec2(IconX + IconSize, IconY + IconSize),
                               ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), Tint);
        }
        else
        {
            const ImVec2 LabelSize = ImGui::CalcTextSize(FallbackLabel);
            DrawList->AddText(ImVec2(Min.x + ((Max.x - Min.x) - LabelSize.x) * 0.5f, Min.y + ((Max.y - Min.y) - LabelSize.y) * 0.5f),
                              ImGui::GetColorU32(ImGuiCol_Text), FallbackLabel);
        }

        if (Tooltip && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Tooltip);
        }

        return bClicked;
    }

    bool DrawIconLabelButton(const char *Id, const char *IconKey, const char *FallbackLabel, const char *Tooltip, const ImVec2 &Size,
                             ImU32 Tint = IM_COL32_WHITE)
    {
        const float  Width = Size.x > 0.0f ? Size.x : 140.0f;
        const float  Height = Size.y > 0.0f ? Size.y : 24.0f;
        ImGui::InvisibleButton(Id, ImVec2(Width, Height));
        const bool   bClicked = ImGui::IsItemClicked();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        ImDrawList  *DrawList = ImGui::GetWindowDrawList();
        const bool   bHeld = ImGui::IsItemActive();
        const bool   bHovered = ImGui::IsItemHovered();
        ImU32        BackgroundColor = ImGui::GetColorU32(ImGuiCol_Button);
        if (bHeld)
        {
            BackgroundColor = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        }
        else if (bHovered)
        {
            BackgroundColor = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        }
        DrawList->AddRectFilled(Min, Max, BackgroundColor, ImGui::GetStyle().FrameRounding);
        DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding);
        float        CursorX = Min.x + 8.0f;
        const float  CenterY = Min.y + (Max.y - Min.y) * 0.5f;

        if (ID3D11ShaderResourceView *Icon = GetEditorIcon(IconKey))
        {
            const float IconSize = (std::min)(ImGui::GetItemRectSize().y - 8.0f, 14.0f);
            DrawList->AddImage(reinterpret_cast<ImTextureID>(Icon), ImVec2(CursorX, CenterY - IconSize * 0.5f),
                               ImVec2(CursorX + IconSize, CenterY + IconSize * 0.5f), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), Tint);
            CursorX += IconSize + 8.0f;
        }

        const ImVec2 LabelSize = ImGui::CalcTextSize(FallbackLabel);
        DrawList->AddText(ImVec2(CursorX, CenterY - LabelSize.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), FallbackLabel);

        if (Tooltip && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", Tooltip);
        }

        return bClicked;
    }

    bool BeginDetailsSection(const char *SectionName)
    {
        const std::string HeaderId = std::string(SectionName) + "##DetailsSection";
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.76f, 0.76f, 0.78f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
        const bool bOpen = ImGui::CollapsingHeader(HeaderId.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        return bOpen;
    }

    const char *GetComponentIconKey(const UActorComponent *Component)
    {
        if (Component && Component->IsA<UStaticMeshComponent>())
        {
            return "Editor.Icon.Component.StaticMesh";
        }
        return "Editor.Icon.Component";
    }

    void DrawLastTreeNodeIcon(const UActorComponent *Component)
    {
        if (ID3D11ShaderResourceView *Icon = GetEditorIcon(GetComponentIconKey(Component)))
        {
            const ImVec2 Min = ImGui::GetItemRectMin();
            const float  IconSize = 14.0f;
            const float  X = Min.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x + 1.0f;
            const float  Y = Min.y + (ImGui::GetItemRectSize().y - IconSize) * 0.5f;
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(Icon), ImVec2(X, Y), ImVec2(X + IconSize, Y + IconSize));
        }
    }

    constexpr const char *ComponentTreeLabelPadding = "    ";

    FString ToLowerCopy(FString Value)
    {
        std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
        return Value;
    }

    bool ContainsCaseInsensitive(const FString &Haystack, const FString &Needle)
    {
        if (Needle.empty())
        {
            return true;
        }

        return ToLowerCopy(Haystack).find(ToLowerCopy(Needle)) != FString::npos;
    }

    bool ShouldHideInComponentTree(const UActorComponent *Component, bool bShowEditorOnlyComponents)
    {
        if (!Component)
        {
            return true;
        }

        return Component->IsHiddenInComponentTree() && !(bShowEditorOnlyComponents && Component->IsEditorOnlyComponent());
    }

    struct FComponentClassGroup
    {
        const char      *Label = nullptr;
        UClass          *AnchorClass = nullptr;
        TArray<UClass *> Classes;
    };

    constexpr ImVec4 AddComponentGroupHeaderTextColor = ImVec4(0.82f, 0.82f, 0.84f, 1.0f);
    constexpr ImVec4 DetailsVectorLabelColor = ImVec4(0.83f, 0.84f, 0.87f, 1.0f);
    constexpr float  DetailsVectorLabelWidth = 76.0f;

    void PushDetailsFieldStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, PopupPalette::FieldBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, PopupPalette::FieldHoverBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, PopupPalette::FieldActiveBg);
        ImGui::PushStyleColor(ImGuiCol_Border, PopupPalette::FieldBorder);
    }

    void PopDetailsFieldStyle() { ImGui::PopStyleColor(4); }

    void AddComponentClassGroup(TArray<FComponentClassGroup> &Groups, const char *Label, UClass *AnchorClass)
    {
        FComponentClassGroup Group;
        Group.Label = Label;
        Group.AnchorClass = AnchorClass;
        Groups.push_back(Group);
    }

    UClass *FindComponentClassGroupAnchor(UClass *ComponentClass, const TArray<FComponentClassGroup> &Groups)
    {
        if (!ComponentClass)
        {
            return nullptr;
        }

        // UTextRenderComponent는 C++ 상속은 Billboard지만 RTTI 등록 부모가 Primitive라서 명시적으로 묶는다.
        if (ComponentClass == UTextRenderComponent::StaticClass())
        {
            return UBillboardComponent::StaticClass();
        }

        for (const FComponentClassGroup &Group : Groups)
        {
            if (Group.AnchorClass && ComponentClass->IsA(Group.AnchorClass))
            {
                return Group.AnchorClass;
            }
        }

        return nullptr;
    }

    const TArray<FComponentClassGroup> &GetCachedAddComponentClassGroups()
    {
        static TArray<FComponentClassGroup> CachedGroups;
        static size_t                       CachedClassCount = 0;

        const TArray<UClass *> &AllClasses = UClass::GetAllClasses();
        if (!CachedGroups.empty() && CachedClassCount == AllClasses.size())
        {
            return CachedGroups;
        }

        CachedGroups.clear();
        CachedClassCount = AllClasses.size();

        AddComponentClassGroup(CachedGroups, "LIGHT", ULightComponentBase::StaticClass());
        AddComponentClassGroup(CachedGroups, "MOVEMENT", UMovementComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "EFFECTS", UBillboardComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "PRIMITIVE", UPrimitiveComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "SCENE", USceneComponent::StaticClass());
        AddComponentClassGroup(CachedGroups, "OTHER", nullptr);

        for (UClass *Cls : AllClasses)
        {
            if (!Cls->IsA(UActorComponent::StaticClass()) || Cls->HasAnyClassFlags(CF_HiddenInComponentList))
            {
                continue;
            }

            UClass *AnchorClass = FindComponentClassGroupAnchor(Cls, CachedGroups);
            for (FComponentClassGroup &Group : CachedGroups)
            {
                if ((AnchorClass && Group.AnchorClass == AnchorClass) || (!AnchorClass && Group.AnchorClass == nullptr))
                {
                    Group.Classes.push_back(Cls);
                    break;
                }
            }
        }

        for (FComponentClassGroup &Group : CachedGroups)
        {
            std::sort(Group.Classes.begin(), Group.Classes.end(),
                      [](const UClass *A, const UClass *B) { return strcmp(A->GetName(), B->GetName()) < 0; });
        }

        return CachedGroups;
    }

    // ===========================================
    // Lua Script 관련 상세 패널 액션
    // ===========================================

    void RenderScriptComponentControls(UScriptComponent *ScriptComponent)
    {
        if (!ScriptComponent)
        {
            return;
        }

        // ScriptComponent는 단순 속성 표시만으로 끝나지 않고
        // 파일 생성/열기/reload 같은 명시적 액션이 필요해서 별도 버튼 묶음을 둔다.
        ImGui::Separator();
        ImGui::TextUnformatted("Lua Script");

        if (ImGui::Button("Create Script"))
        {
            ScriptComponent->CreateScript();
        }

        ImGui::SameLine();
        if (ImGui::Button("Edit Script"))
        {
            ScriptComponent->OpenScript();
        }

        ImGui::SameLine();
        if (ImGui::Button("Reload Script"))
        {
            ScriptComponent->RefreshScriptProperties();
        }
    }

    bool IsBehaviorPropertyName(const FString &Name) { return Name == "bTickEnable" || Name == "bEditorOnly"; }

    bool IsVisibilityPropertyName(const FString &Name)
    {
        return Name == "Visible" || Name == "Cast Shadow" || Name == "Two Sided Shadow" || Name == "Is Collidable" ||
               Name == "Generates Overlap Event";
    }
} // namespace

static FString RemoveExtension(const FString &Path)
{
    size_t DotPos = Path.find_last_of('.');
    if (DotPos == FString::npos)
    {
        return Path;
    }
    return Path.substr(0, DotPos);
}

static FString GetStemFromPath(const FString &Path)
{
    size_t  SlashPos = Path.find_last_of("/\\");
    FString FileName = (SlashPos == FString::npos) ? Path : Path.substr(SlashPos + 1);
    return RemoveExtension(FileName);
}

static FString MakeAssetPreviewLabel(const FString &Path)
{
    if (Path.empty() || Path == "None")
    {
        return "None";
    }

    return GetStemFromPath(Path);
}

FString FEditorDetailsWidget::GetDisplayPropertyLabel(const FString &RawName)
{
    if (RawName == "bTickEnable")
        return "Tick Enabled";
    if (RawName == "bEditorOnly")
        return "Editor Only";
    if (RawName == "Is Collidable")
        return "Collision Enabled";
    if (RawName == "Generates Overlap Event")
        return "Generate Overlap Events";

    FString Result;
    Result.reserve(RawName.size() + 8);
    size_t StartIndex = 0;
    if (RawName.size() > 1 && RawName[0] == 'b' && std::isupper(static_cast<unsigned char>(RawName[1])))
    {
        StartIndex = 1;
    }

    for (size_t i = StartIndex; i < RawName.size(); ++i)
    {
        const char C = RawName[i];
        if ((C == '_' || C == '-') && !Result.empty() && Result.back() != ' ')
        {
            Result.push_back(' ');
            continue;
        }

        const bool bIsUpper = std::isupper(static_cast<unsigned char>(C)) != 0;
        const bool bNeedsSpace = i > StartIndex && bIsUpper && Result.back() != ' ' && !std::isupper(static_cast<unsigned char>(RawName[i - 1]));
        if (bNeedsSpace)
        {
            Result.push_back(' ');
        }
        Result.push_back(C);
    }

    return Result.empty() ? RawName : Result;
}

FString FEditorDetailsWidget::GetPropertySectionName(const FPropertyDescriptor &Prop)
{
    if (Prop.Name == "Location" || Prop.Name == "Rotation" || Prop.Name == "Scale")
    {
        return "Transform";
    }
    if (Prop.Type == EPropertyType::StaticMeshRef)
    {
        return "Static Mesh";
    }
    if (Prop.Type == EPropertyType::MaterialSlot)
    {
        return "Materials";
    }
    if (IsVisibilityPropertyName(Prop.Name))
    {
        return "Visibility";
    }
    if (IsBehaviorPropertyName(Prop.Name))
    {
        return "Behavior";
    }
    return "Default";
}

bool FEditorDetailsWidget::DrawColoredFloat3(const char *Label, float Values[3], float Speed)
{
    ImGui::PushID(Label);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, DetailsVectorLabelColor);
    ImGui::TextUnformatted(Label);
    ImGui::PopStyleColor();
    ImGui::SameLine(DetailsVectorLabelWidth);

    const float  AvailableWidth = ImGui::GetContentRegionAvail().x;
    const float  Width = (AvailableWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
    const ImVec4 AxisColors[3] = {ImVec4(0.85f, 0.22f, 0.22f, 1.0f), ImVec4(0.36f, 0.74f, 0.25f, 1.0f), ImVec4(0.23f, 0.54f, 0.92f, 1.0f)};

    bool bChanged = false;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (Axis > 0)
        {
            ImGui::SameLine();
        }
        const ImVec2 Start = ImGui::GetCursorScreenPos();
        const float  BarWidth = 3.0f;
        const float  Spacing = 4.0f;
        ImGui::GetWindowDrawList()->AddRectFilled(Start, ImVec2(Start.x + BarWidth, Start.y + ImGui::GetFrameHeight()),
                                                  ImGui::ColorConvertFloat4ToU32(AxisColors[Axis]), 2.0f);
        ImGui::SetCursorScreenPos(ImVec2(Start.x + BarWidth + Spacing, Start.y));
        PushDetailsFieldStyle();
        ImGui::SetNextItemWidth(Width - (BarWidth + Spacing));
        bChanged |= ImGui::DragFloat(Axis == 0 ? "##X" : Axis == 1 ? "##Y" : "##Z", &Values[Axis], Speed, 0.0f, 0.0f, "%.3f");
        PopDetailsFieldStyle();
    }
    ImGui::PopID();
    return bChanged;
}

bool FEditorDetailsWidget::DrawColoredFloat4(const char *Label, float Values[4], float Speed) { return ImGui::DragFloat4(Label, Values, Speed); }

FString FEditorPropertyWidget::OpenObjFileDialog()
{
    wchar_t FilePath[MAX_PATH] = {};

    OPENFILENAMEW Ofn = {};
    Ofn.lStructSize = sizeof(Ofn);
    Ofn.hwndOwner = nullptr;
    Ofn.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
    Ofn.lpstrFile = FilePath;
    Ofn.nMaxFile = MAX_PATH;
    Ofn.lpstrTitle = L"Import OBJ Mesh";
    Ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&Ofn))
    {
        std::filesystem::path AbsPath = std::filesystem::path(FilePath).lexically_normal();
        std::filesystem::path RootPath = std::filesystem::path(FPaths::RootDir());
        std::filesystem::path RelPath = AbsPath.lexically_relative(RootPath);

        // 상대 경로 변환 실패 시 (드라이브가 다른 경우 등) 절대 경로를 그대로 반환
        if (RelPath.empty() || RelPath.wstring().starts_with(L".."))
        {
            return FPaths::ToUtf8(AbsPath.generic_wstring());
        }
        return FPaths::ToUtf8(RelPath.generic_wstring());
    }

    return FString();
}

void FEditorPropertyWidget::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_Once);

    FEditorSettings &Settings = FEditorSettings::Get();
    if (!Settings.UI.bProperty)
    {
        return;
    }

    constexpr const char *PanelIconKey = "Editor.Icon.Panel.Details";
    const std::string     WindowTitle = EditorPanelTitleUtils::MakeClosablePanelTitle("Details", PanelIconKey);
    ImGui::Begin(WindowTitle.c_str());
    EditorPanelTitleUtils::DrawPanelTitleIcon(PanelIconKey);
    EditorPanelTitleUtils::DrawSmallPanelCloseButton("    Details", Settings.UI.bProperty, "x##CloseDetails");

    FSelectionManager &Selection = EditorEngine->GetSelectionManager();
    AActor            *PrimaryActor = bSelectionLocked ? LockedActor : Selection.GetPrimarySelection();
    if (!PrimaryActor)
    {
        if (bSelectionLocked)
        {
            bSelectionLocked = false;
            LockedActor = nullptr;
        }
        SelectedComponent = nullptr;
        ScriptPathEditComponent = nullptr;
        LastSelectedActor = nullptr;
        bActorSelected = true;
        bScriptPathEditActive = false;
        ScriptPathEditBuffer[0] = '\0';
        ImGui::Text("Select an object to view details.");
        ImGui::End();
        return;
    }

    // Actor 선택이 바뀌면 초기화
    if (PrimaryActor != LastSelectedActor)
    {
        SelectedComponent = nullptr;
        ScriptPathEditComponent = nullptr;
        LastSelectedActor = PrimaryActor;
        bActorSelected = true;
        bEditingActorName = false;
        bScriptPathEditActive = false;
        ScriptPathEditBuffer[0] = '\0';
    }

    TArray<AActor *>        DisplayedActors;
    const TArray<AActor *> &SelectionActors = Selection.GetSelectedActors();
    const TArray<AActor *> *SelectedActorsPtr = &SelectionActors;
    if (bSelectionLocked)
    {
        DisplayedActors.push_back(PrimaryActor);
        SelectedActorsPtr = &DisplayedActors;
    }
    const TArray<AActor *> &SelectedActors = *SelectedActorsPtr;
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    RenderHeader(PrimaryActor, SelectedActors);

    constexpr float ResizeHandleHeight = 8.0f;
    constexpr float MinTreeHeight = 80.0f;
    constexpr float MinDetailsHeight = 50.0f;
    const float     AvailableHeight = ImGui::GetContentRegionAvail().y;
    const float     MaxTreeHeight = (std::max)(MinTreeHeight, AvailableHeight - MinDetailsHeight - ResizeHandleHeight);
    ComponentTreeHeight = (std::clamp)(ComponentTreeHeight, MinTreeHeight, MaxTreeHeight);

    RenderComponentTree(PrimaryActor, ComponentTreeHeight);

    const ImVec2 HandleCursor = ImGui::GetCursorScreenPos();
    const float  HandleWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
    ImGui::InvisibleButton("##ComponentTreeResizeHandle", ImVec2(HandleWidth, ResizeHandleHeight));
    const bool bHandleHovered = ImGui::IsItemHovered();
    const bool bHandleActive = ImGui::IsItemActive();
    if (bHandleHovered || bHandleActive)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (bHandleActive)
    {
        ComponentTreeHeight = (std::clamp)(ComponentTreeHeight + ImGui::GetIO().MouseDelta.y, MinTreeHeight, MaxTreeHeight);
    }

    ImDrawList *DrawList = ImGui::GetWindowDrawList();
    const float LineY = HandleCursor.y + ResizeHandleHeight * 0.5f;
    const ImU32 LineColor = ImGui::GetColorU32(bHandleActive    ? ImVec4(0.22f, 0.55f, 0.95f, 1.0f)
                                               : bHandleHovered ? ImVec4(0.45f, 0.48f, 0.55f, 1.0f)
                                                                : ImVec4(0.26f, 0.28f, 0.32f, 1.0f));
    DrawList->AddLine(ImVec2(HandleCursor.x, LineY), ImVec2(HandleCursor.x + HandleWidth, LineY), LineColor, 2.0f);

    float ScrollHeight = ImGui::GetContentRegionAvail().y;
    if (ScrollHeight < MinDetailsHeight)
        ScrollHeight = MinDetailsHeight;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, PopupPalette::SurfaceBg);
    ImGui::BeginChild("##Details", ImVec2(0, ScrollHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        RenderDetails(PrimaryActor, SelectedActors);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
}

void FEditorDetailsWidget::RenderDetailsFilterBar(const TArray<const char *> &AvailableSections)
{
    const float SearchWidth = ImGui::GetContentRegionAvail().x;
    DrawSearchInputWithIcon("##DetailsSearch", "Search", DetailSearchBuffer, sizeof(DetailSearchBuffer), SearchWidth);
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 5.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    for (int32 SectionIndex = -1; SectionIndex < static_cast<int32>(AvailableSections.size()); ++SectionIndex)
    {
        const char       *Label = SectionIndex < 0 ? "All" : AvailableSections[SectionIndex];
        const std::string ButtonId = std::string(Label) + "##DetailsFilter";
        const bool        bActive = ActiveSectionFilter == Label;
        if (SectionIndex >= 0)
        {
            ImGui::SameLine(0.0f, 4.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Button, bActive ? ImVec4(64.0f / 255.0f, 64.0f / 255.0f, 64.0f / 255.0f, 1.0f) : ImVec4(36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bActive ? ImVec4(76.0f / 255.0f, 76.0f / 255.0f, 76.0f / 255.0f, 1.0f) : ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, bActive ? ImVec4(52.0f / 255.0f, 52.0f / 255.0f, 52.0f / 255.0f, 1.0f) : ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f));
        if (ImGui::Button(ButtonId.c_str()))
        {
            ActiveSectionFilter = Label;
        }
        ImGui::PopStyleColor(4);
    }
    ImGui::PopStyleVar(3);
}

bool FEditorDetailsWidget::SectionMatchesSearch(const char *SectionName, const TArray<FPropertyDescriptor> &Props, const TArray<int32> &Indices) const
{
    if (DetailSearchBuffer[0] == '\0')
    {
        return true;
    }

    const FString Query = DetailSearchBuffer;
    if (ContainsCaseInsensitive(SectionName, Query))
    {
        return true;
    }

    for (int32 PropIndex : Indices)
    {
        if (PropIndex < 0 || PropIndex >= static_cast<int32>(Props.size()))
        {
            continue;
        }

        if (ContainsCaseInsensitive(GetDisplayPropertyLabel(Props[PropIndex].Name), Query) || ContainsCaseInsensitive(Props[PropIndex].Name, Query))
        {
            return true;
        }
    }

    return false;
}

bool FEditorDetailsWidget::ShouldDisplaySection(const char *SectionName, const TArray<FPropertyDescriptor> &Props, const TArray<int32> &Indices) const
{
    if (Indices.empty())
    {
        return false;
    }

    if (ActiveSectionFilter != "All" && ActiveSectionFilter != SectionName)
    {
        return false;
    }

    return SectionMatchesSearch(SectionName, Props, Indices);
}

void FEditorDetailsWidget::CommitActorNameEdit(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }

    FString NewName = ActorNameBuffer;
    if (NewName.empty())
    {
        NewName = Actor->GetClass()->GetName();
    }

    EditorEngine->BeginTrackedSceneChange();
    Actor->SetFName(FName(NewName));
    EditorEngine->CommitTrackedSceneChange();
    bEditingActorName = false;
}

void FEditorDetailsWidget::RenderHeader(AActor *PrimaryActor, const TArray<AActor *> &SelectedActors)
{
    const int32 SelectionCount = static_cast<int32>(SelectedActors.size());

    if (!bEditingActorName)
    {
        strncpy_s(ActorNameBuffer, PrimaryActor->GetFName().ToString().c_str(), _TRUNCATE);
    }

    if (SelectionCount <= 1)
    {
        const float AvailableWidth = ImGui::GetContentRegionAvail().x;
        const float InputWidth = (std::clamp)(AvailableWidth - 84.0f, 120.0f, 240.0f);
        if (ID3D11ShaderResourceView *ActorIcon = GetEditorIcon(GetActorHeaderIconKey(PrimaryActor)))
        {
            ImGui::Image(ActorIcon, ImVec2(16.0f, 16.0f));
            ImGui::SameLine(0.0f, 6.0f);
        }
        if (bEditingActorName)
        {
            ImGui::SetWindowFontScale(1.18f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.42f, 0.45f, 0.90f));
            ImGui::SetNextItemWidth(InputWidth);
            if (ImGui::InputText("##ActorNameEdit", ActorNameBuffer, sizeof(ActorNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                CommitActorNameEdit(PrimaryActor);
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                CommitActorNameEdit(PrimaryActor);
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.0f);
        }
        else
        {
            ImGui::SetWindowFontScale(1.18f);
            ImGui::TextUnformatted(PrimaryActor->GetFName().ToString().c_str());
            ImGui::SetWindowFontScale(1.0f);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                bEditingActorName = true;
            }
        }
    }
    else
    {
        ImGui::Text("%s (+%d)", PrimaryActor->GetFName().ToString().c_str(), SelectionCount - 1);
    }

    if (SelectionCount <= 1)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", PrimaryActor->GetClass()->GetName());
    }

    ImGui::SameLine((std::max)(0.0f, ImGui::GetWindowContentRegionMax().x - 130.0f));
    RenderAddComponentButton(PrimaryActor);
    ImGui::SameLine(0.0f, 6.0f);
    const bool  bLocked = bSelectionLocked && LockedActor == PrimaryActor;
    const char *LockIconKey = bLocked ? "Editor.Icon.Locked" : "Editor.Icon.Unlocked";
    const char *LockTooltip = bLocked ? "Unlock Details Panel" : "Lock Details Panel to Selection";
    const ImU32 LockTint = bLocked ? IM_COL32(94, 170, 255, 255) : IM_COL32(215, 215, 215, 255);
    if (DrawHeaderIconButton("##LockDetailsSelection", LockIconKey, bLocked ? "Unlock" : "Lock", LockTooltip, ImVec2(28.0f, 0.0f), LockTint))
    {
        if (bLocked)
        {
            bSelectionLocked = false;
            LockedActor = nullptr;
        }
        else
        {
            bSelectionLocked = true;
            LockedActor = PrimaryActor;
        }
    }
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
}

void FEditorDetailsWidget::RenderAddComponentButton(AActor *Actor)
{
    if (!Actor)
    {
        return;
    }

    if (DrawAddHeaderButton("##AddComponentButton", ImVec2(70.0f, 0.0f)))
    {
        ImGui::OpenPopup("##AddComponentPopup");
    }

    if (!ImGui::BeginPopup("##AddComponentPopup"))
    {
        return;
    }

    const TArray<FComponentClassGroup> &Groups = GetCachedAddComponentClassGroups();
    for (const FComponentClassGroup &Group : Groups)
    {
        if (Group.Classes.empty())
        {
            continue;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, AddComponentGroupHeaderTextColor);
        ImGui::SeparatorText(Group.Label);
        ImGui::PopStyleColor();
        for (UClass *Cls : Group.Classes)
        {
            if (!ImGui::MenuItem(Cls->GetName()))
            {
                continue;
            }

            EditorEngine->BeginTrackedSceneChange();
            UWorld *World = Actor->GetWorld();
            if (World)
            {
                World->BeginDeferredPickingBVHUpdate();
            }
            UActorComponent *Comp = Actor->AddComponentByClass(Cls);
            if (!Comp)
            {
                if (World)
                {
                    World->EndDeferredPickingBVHUpdate();
                }
                EditorEngine->CancelTrackedSceneChange();
                continue;
            }

            if (Cls->IsA(USceneComponent::StaticClass()))
            {
                USceneComponent *Root = Actor->GetRootComponent();
                if (SelectedComponent != nullptr && SelectedComponent->GetClass()->IsA(USceneComponent::StaticClass()))
                {
                    Cast<USceneComponent>(Comp)->AttachToComponent(Cast<USceneComponent>(SelectedComponent));
                }
                else if (Root)
                {
                    Cast<USceneComponent>(Comp)->AttachToComponent(Root);
                }

                if (Comp->IsA<ULightComponentBase>())
                {
                    Cast<ULightComponentBase>(Comp)->EnsureEditorBillboard();
                }
                else if (Comp->IsA<UDecalComponent>())
                {
                    Cast<UDecalComponent>(Comp)->EnsureEditorBillboard();
                }
                else if (Comp->IsA<UHeightFogComponent>())
                {
                    Cast<UHeightFogComponent>(Comp)->EnsureEditorBillboard();
                }
            }

            if (World)
            {
                World->EndDeferredPickingBVHUpdate();
            }
            EditorEngine->CommitTrackedSceneChange();
            ImGui::CloseCurrentPopup();
            break;
        }
    }

    ImGui::EndPopup();
}

void FEditorPropertyWidget::RenderDetails(AActor *PrimaryActor, const TArray<AActor *> &SelectedActors)
{
    if (bActorSelected)
    {
        RenderActorProperties(PrimaryActor, SelectedActors);
    }
    else if (SelectedComponent && SelectedActors.size() >= 2)
    {
        // 다중 선택 시 모든 액터의 타입이 동일한지 검증
        UClass *PrimaryClass = PrimaryActor->GetClass();
        bool    bAllSameType = true;
        for (const AActor *Actor : SelectedActors)
        {
            if (Actor && Actor->GetClass() != PrimaryClass)
            {
                bAllSameType = false;
                break;
            }
        }

        if (!bAllSameType)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Multi-edit unavailable");
            ImGui::TextWrapped("Selected actors have different types. "
                               "Multi-component editing requires all selected actors to be the same type.");

            ImGui::Spacing();
            ImGui::TextDisabled("Primary: %s", PrimaryClass->GetName());
            for (const AActor *Actor : SelectedActors)
            {
                if (Actor && Actor->GetClass() != PrimaryClass)
                {
                    ImGui::TextDisabled("  Mismatch: %s (%s)", Actor->GetFName().ToString().c_str(), Actor->GetClass()->GetName());
                }
            }
        }
        else
        {
            RenderComponentProperties(PrimaryActor, SelectedActors);
        }
    }
    else if (SelectedComponent)
    {
        RenderComponentProperties(PrimaryActor, SelectedActors);
    }
    else
    {
        ImGui::TextDisabled("Select an actor or component to view details.");
    }
}

void FEditorPropertyWidget::RenderActorProperties(AActor *PrimaryActor, const TArray<AActor *> &SelectedActors)
{
    TArray<const char *> AvailableSections;
    if (PrimaryActor->GetRootComponent())
    {
        AvailableSections.push_back("Transform");
    }
    AvailableSections.push_back("Visibility");
    RenderDetailsFilterBar(AvailableSections);

    const FString SearchQuery = DetailSearchBuffer;
    const bool    bMatchesTransform = DetailSearchBuffer[0] == '\0' || ContainsCaseInsensitive("Transform", SearchQuery) ||
                                   ContainsCaseInsensitive("Location", SearchQuery) || ContainsCaseInsensitive("Rotation", SearchQuery) ||
                                   ContainsCaseInsensitive("Scale", SearchQuery);
    const bool bMatchesVisibility =
        DetailSearchBuffer[0] == '\0' || ContainsCaseInsensitive("Visibility", SearchQuery) || ContainsCaseInsensitive("Visible", SearchQuery);

    bool bRenderedAnySection = false;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 3.0f));
    if (PrimaryActor->GetRootComponent())
    {
        if ((ActiveSectionFilter == "All" || ActiveSectionFilter == "Transform") && bMatchesTransform)
        {
            bRenderedAnySection = true;
            if (BeginDetailsSection("Transform"))
            {
                FVector Pos = PrimaryActor->GetActorLocation();
                float   PosArray[3] = {Pos.X, Pos.Y, Pos.Z};

                USceneComponent *RootComp = PrimaryActor->GetRootComponent();

                FVector Scale = PrimaryActor->GetActorScale();
                float   ScaleArray[3] = {Scale.X, Scale.Y, Scale.Z};

                if (DrawColoredFloat3("Location", PosArray, 0.1f))
                {
                    EditorEngine->BeginTrackedSceneChange();
                    FVector Delta = FVector(PosArray[0], PosArray[1], PosArray[2]) - Pos;
                    for (AActor *Actor : SelectedActors)
                    {
                        if (Actor)
                            Actor->AddActorWorldOffset(Delta);
                    }
                    EditorEngine->GetGizmo()->UpdateGizmoTransform();
                    EditorEngine->CommitTrackedSceneChange();
                }
                {
                    FRotator &CachedRot = RootComp->GetCachedEditRotator();
                    FRotator  PrevRot = CachedRot;
                    float     RotXYZ[3] = {CachedRot.Roll, CachedRot.Pitch, CachedRot.Yaw};

                    if (DrawColoredFloat3("Rotation", RotXYZ, 0.1f))
                    {
                        EditorEngine->BeginTrackedSceneChange();
                        CachedRot.Roll = RotXYZ[0];
                        CachedRot.Pitch = RotXYZ[1];
                        CachedRot.Yaw = RotXYZ[2];

                        if (SelectedActors.size() > 1)
                        {
                            FRotator Delta = CachedRot - PrevRot;
                            for (AActor *Actor : SelectedActors)
                            {
                                if (!Actor || Actor == PrimaryActor)
                                    continue;
                                USceneComponent *Root = Actor->GetRootComponent();
                                if (Root)
                                {
                                    FRotator Other = Root->GetCachedEditRotator();
                                    Root->SetRelativeRotation(Other + Delta);
                                }
                            }
                        }
                        RootComp->ApplyCachedEditRotator();
                        EditorEngine->GetGizmo()->UpdateGizmoTransform();
                        EditorEngine->CommitTrackedSceneChange();
                    }
                }
                if (DrawColoredFloat3("Scale", ScaleArray, 0.1f))
                {
                    EditorEngine->BeginTrackedSceneChange();
                    FVector Delta = FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]) - Scale;
                    for (AActor *Actor : SelectedActors)
                    {
                        if (Actor)
                            Actor->SetActorScale(Actor->GetActorScale() + Delta);
                    }
                    EditorEngine->CommitTrackedSceneChange();
                }
            }
        }
    }

    if ((ActiveSectionFilter == "All" || ActiveSectionFilter == "Visibility") && bMatchesVisibility)
    {
        bRenderedAnySection = true;
        if (BeginDetailsSection("Visibility"))
        {
            bool bVisible = PrimaryActor->IsVisible();
            if (ImGui::Checkbox("Visible", &bVisible))
            {
                EditorEngine->BeginTrackedSceneChange();
                PrimaryActor->SetVisible(bVisible);
                EditorEngine->CommitTrackedSceneChange();
            }
        }
    }
    ImGui::PopStyleVar();

    if (!bRenderedAnySection)
    {
        ImGui::TextDisabled("No matching sections.");
    }
}

void FEditorPropertyWidget::RenderComponentTree(AActor *Actor, float Height)
{
    if (SelectedComponent && ShouldHideInComponentTree(SelectedComponent, bShowEditorOnlyComponents))
    {
        SelectedComponent = nullptr;
        bActorSelected = true;
    }

    USceneComponent *Root = Actor->GetRootComponent();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 2.0f));
    ImGui::BeginChild("##ComponentTreeBox", ImVec2(0.0f, Height), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if (Root)
    {
        RenderSceneComponentNode(Root);
    }

    for (UActorComponent *Comp : Actor->GetComponents())
    {
        if (!Comp)
            continue;
        if (Comp->IsA<USceneComponent>())
            continue;
        if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents))
            continue;

        FString       Name = Comp->GetFName().ToString();
        const FString TypeName = Comp->GetClass()->GetName();
        const FString DefaultNamePrefix = TypeName + "_";
        const bool    bUseTypeAsLabel = Name.empty() || Name == TypeName || Name.rfind(DefaultNamePrefix, 0) == 0;
        FString       LabelText = bUseTypeAsLabel ? TypeName : Name;

        ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        const bool         bIsSelected = !bActorSelected && SelectedComponent == Comp;
        if (bIsSelected)
        {
            Flags |= ImGuiTreeNodeFlags_Selected;
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.06f, 0.33f, 0.75f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.09f, 0.39f, 0.84f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.05f, 0.28f, 0.67f, 1.0f));
        }

        ImGui::TreeNodeEx(Comp, Flags, "%s%s", ComponentTreeLabelPadding, LabelText.c_str());
        if (bIsSelected)
        {
            ImGui::PopStyleColor(3);
        }
        DrawLastTreeNodeIcon(Comp);
        if (ImGui::IsItemClicked())
        {
            SelectedComponent = Comp;
            bActorSelected = false;
        }
        RenderComponentContextMenu(Actor, Comp);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(3);
}

void FEditorPropertyWidget::RenderSceneComponentNode(USceneComponent *Comp)
{
    if (!Comp)
        return;
    if (ShouldHideInComponentTree(Comp, bShowEditorOnlyComponents))
        return;

    FString Name = Comp->GetFName().ToString();
    if (Name.empty())
        Name = Comp->GetClass()->GetName();

    const auto &Children = Comp->GetChildren();
    bool        bHasVisibleChildren = false;
    for (USceneComponent *Child : Children)
    {
        if (Child && !ShouldHideInComponentTree(Child, bShowEditorOnlyComponents))
        {
            bHasVisibleChildren = true;
            break;
        }
    }

    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (!bHasVisibleChildren)
        Flags |= ImGuiTreeNodeFlags_Leaf;
    const bool bIsSelected = !bActorSelected && SelectedComponent == Comp;
    if (bIsSelected)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.06f, 0.33f, 0.75f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.09f, 0.39f, 0.84f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.05f, 0.28f, 0.67f, 1.0f));
    }

    bool    bIsRoot = (Comp->GetParent() == nullptr);
    FString LabelText;
    if (bIsRoot)
    {
        LabelText = "[Root] ";
    }
    LabelText += Name;
    bool bOpen = ImGui::TreeNodeEx(Comp, Flags, "%s%s (%s)", ComponentTreeLabelPadding, LabelText.c_str(), Comp->GetClass()->GetName());
    if (bIsSelected)
    {
        ImGui::PopStyleColor(3);
    }
    DrawLastTreeNodeIcon(Comp);

    if (ImGui::IsItemClicked())
    {
        SelectedComponent = Comp;
        bActorSelected = false;
        EditorEngine->GetSelectionManager().SelectComponent(Comp);
    }
    RenderComponentContextMenu(Comp->GetOwner(), Comp);

    // 컴포넌트 트리에서 간단하게 드래그 앤 드랍으로 부모-자식 관계 변경 가능하도록 지원
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("SCENE_COMPONENT_REPARENT", &Comp, sizeof(USceneComponent *));
        ImGui::Text("Reparent %s", Name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SCENE_COMPONENT_REPARENT"))
        {
            USceneComponent *DraggedComp = *(USceneComponent **)payload->Data;
            if (DraggedComp && DraggedComp != Comp)
            {
                // Circular dependency check: Ensure Comp is not a child of DraggedComp
                bool             bIsChildOfDragged = false;
                USceneComponent *Check = Comp;
                while (Check)
                {
                    if (Check == DraggedComp)
                    {
                        bIsChildOfDragged = true;
                        break;
                    }
                    Check = Check->GetParent();
                }

                if (!bIsChildOfDragged)
                {
                    DraggedComp->SetParent(Comp);
                    if (EditorEngine && EditorEngine->GetGizmo())
                    {
                        EditorEngine->GetGizmo()->UpdateGizmoTransform();
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (bOpen)
    {
        for (USceneComponent *Child : Children)
        {
            RenderSceneComponentNode(Child);
        }
        ImGui::TreePop();
    }
}

void FEditorPropertyWidget::RenderComponentContextMenu(AActor *Actor, UActorComponent *Component)
{
    if (!Actor || !Component)
    {
        return;
    }

    if (!ImGui::BeginPopupContextItem())
    {
        return;
    }

    SelectedComponent = Component;
    bActorSelected = false;
    if (USceneComponent *SceneComponent = Cast<USceneComponent>(Component))
    {
        EditorEngine->GetSelectionManager().SelectComponent(SceneComponent);
    }

    const bool bCanDelete = Component->CanDeleteFromDetails();
    if (!bCanDelete)
    {
        ImGui::BeginDisabled();
    }
    if (DrawIconLabelButton("##DeleteComponentContext", "Editor.Icon.Delete", "Delete", "Delete Component", ImVec2(120.0f, 0.0f),
                            IM_COL32(255, 225, 225, 255)) &&
        bCanDelete)
    {
        DeleteSelectedComponent(Actor);
        ImGui::CloseCurrentPopup();
    }
    if (!bCanDelete)
    {
        ImGui::EndDisabled();
    }

    ImGui::EndPopup();
}

void FEditorPropertyWidget::DeleteSelectedComponent(AActor *Actor)
{
    if (!Actor || !SelectedComponent || !SelectedComponent->CanDeleteFromDetails())
    {
        return;
    }

    EditorEngine->BeginTrackedSceneChange();
    Actor->RemoveComponent(SelectedComponent);
    SelectedComponent = nullptr;
    bActorSelected = true;
    EditorEngine->GetSelectionManager().Select(Actor);
    EditorEngine->CommitTrackedSceneChange();
}

void FEditorPropertyWidget::RenderComponentProperties(AActor *Actor, const TArray<AActor *> &SelectedActors)
{
    // PropertyDescriptor 기반 자동 위젯 렌더링
    TArray<FPropertyDescriptor> Props;
    SelectedComponent->GetEditableProperties(Props);

    // ScriptComponent도 일반 PropertyDescriptor 목록은 제공하지만,
    // create/open/reload 버튼은 범용 descriptor 파이프라인 밖에서 따로 그린다.
    UScriptComponent *ScriptComponent = Cast<UScriptComponent>(SelectedComponent);
    bool              bAnyChanged = false;
    TArray<int32>     TransformIndices;
    TArray<int32>     StaticMeshIndices;
    TArray<int32>     MaterialIndices;
    TArray<int32>     VisibilityIndices;
    TArray<int32>     BehaviorIndices;
    TArray<int32>     DefaultIndices;

    for (int32 i = 0; i < (int32)Props.size(); ++i)
    {
        const FString SectionName = GetPropertySectionName(Props[i]);
        if (SectionName == "Transform")
        {
            TransformIndices.push_back(i);
        }
        else if (SectionName == "Static Mesh")
        {
            StaticMeshIndices.push_back(i);
        }
        else if (SectionName == "Materials")
        {
            MaterialIndices.push_back(i);
        }
        else if (SectionName == "Visibility")
        {
            VisibilityIndices.push_back(i);
        }
        else if (SectionName == "Behavior")
        {
            BehaviorIndices.push_back(i);
        }
        else
        {
            DefaultIndices.push_back(i);
        }
    }

    TArray<const char *> AvailableSections;
    if (!TransformIndices.empty())
        AvailableSections.push_back("Transform");
    if (!StaticMeshIndices.empty())
        AvailableSections.push_back("Static Mesh");
    if (!MaterialIndices.empty() || (SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>()))
        AvailableSections.push_back("Materials");
    if (!VisibilityIndices.empty())
        AvailableSections.push_back("Visibility");
    if (!BehaviorIndices.empty())
        AvailableSections.push_back("Behavior");
    if (!DefaultIndices.empty())
        AvailableSections.push_back("Default");
    RenderDetailsFilterBar(AvailableSections);

    bool bRenderedAnySection = false;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 3.0f));

    if (ShouldDisplaySection("Transform", Props, TransformIndices))
    {
        bRenderedAnySection = true;
        RenderPropertySection("Transform", Props, TransformIndices, SelectedActors, bAnyChanged);
    }
    if (ShouldDisplaySection("Static Mesh", Props, StaticMeshIndices))
    {
        bRenderedAnySection = true;
        RenderPropertySection("Static Mesh", Props, StaticMeshIndices, SelectedActors, bAnyChanged);
    }
    if (ShouldDisplaySection("Materials", Props, MaterialIndices))
    {
        bRenderedAnySection = true;
        RenderPropertySection("Materials", Props, MaterialIndices, SelectedActors, bAnyChanged);
    }
    else if (MaterialIndices.empty() && SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>() &&
             (ActiveSectionFilter == "All" || ActiveSectionFilter == "Materials") && SectionMatchesSearch("Materials", Props, MaterialIndices))
    {
        UStaticMeshComponent *StaticMeshComponent = static_cast<UStaticMeshComponent *>(SelectedComponent);
        StaticMeshComponent->EnsureMaterialSlotsForEditing();
        if (StaticMeshComponent->GetMaterialSlotCount() > 0)
        {
            bRenderedAnySection = true;
            TArray<FPropertyDescriptor> SyntheticProps;
            SyntheticProps.reserve(StaticMeshComponent->GetMaterialSlotCount());

            for (int32 SlotIndex = 0; SlotIndex < StaticMeshComponent->GetMaterialSlotCount(); ++SlotIndex)
            {
                if (FMaterialSlot *Slot = StaticMeshComponent->GetMaterialSlot(SlotIndex))
                {
                    FPropertyDescriptor Desc;
                    Desc.Name = "Element " + std::to_string(SlotIndex);
                    Desc.Type = EPropertyType::MaterialSlot;
                    Desc.ValuePtr = Slot;
                    SyntheticProps.push_back(Desc);
                }
            }

            TArray<int32> SyntheticIndices;
            SyntheticIndices.reserve(SyntheticProps.size());
            for (int32 Index = 0; Index < static_cast<int32>(SyntheticProps.size()); ++Index)
            {
                SyntheticIndices.push_back(Index);
            }

            RenderPropertySection("Materials", SyntheticProps, SyntheticIndices, SelectedActors, bAnyChanged);
        }
        else
        {
            bRenderedAnySection = true;
            if (BeginDetailsSection("Materials"))
            {
                ImGui::TextDisabled("No material slots.");
            }
        }
    }
    if (ShouldDisplaySection("Visibility", Props, VisibilityIndices))
    {
        bRenderedAnySection = true;
        RenderPropertySection("Visibility", Props, VisibilityIndices, SelectedActors, bAnyChanged);
    }
    if (ShouldDisplaySection("Behavior", Props, BehaviorIndices))
    {
        bRenderedAnySection = true;
        RenderPropertySection("Behavior", Props, BehaviorIndices, SelectedActors, bAnyChanged);
    }
    if (ShouldDisplaySection("Default", Props, DefaultIndices))
    {
        bRenderedAnySection = true;
        RenderPropertySection("Default", Props, DefaultIndices, SelectedActors, bAnyChanged);
    }
    ImGui::PopStyleVar();

    if (!bRenderedAnySection)
    {
        ImGui::TextDisabled("No matching sections.");
    }

    // 경로 필드를 먼저 보여준 뒤 스크립트 액션 버튼을 배치해야
    // 사용자가 현재 어떤 파일을 대상으로 동작하는지 바로 확인할 수 있다.
    RenderScriptComponentControls(ScriptComponent);

    // 실제 변경이 있었을 때만 Transform dirty 마킹
    if (bAnyChanged && SelectedComponent->IsA<USceneComponent>())
    {
        static_cast<USceneComponent *>(SelectedComponent)->MarkTransformDirty();
    }
}

void FEditorDetailsWidget::RenderPropertySection(const char *SectionName, TArray<FPropertyDescriptor> &Props, const TArray<int32> &Indices,
                                                 const TArray<AActor *> &SelectedActors, bool &bAnyChanged)
{
    if (Indices.empty())
    {
        return;
    }

    if (!BeginDetailsSection(SectionName))
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.05f, 0.05f, 0.06f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
    const FString Query = DetailSearchBuffer;

    for (int32 PropIndex : Indices)
    {
        if (DetailSearchBuffer[0] != '\0' && !ContainsCaseInsensitive(GetDisplayPropertyLabel(Props[PropIndex].Name), Query) &&
            !ContainsCaseInsensitive(Props[PropIndex].Name, Query))
        {
            continue;
        }

        int32 MutableIndex = PropIndex;
        EditorEngine->BeginTrackedSceneChange();
        const bool bChanged = RenderPropertyWidget(Props, MutableIndex);
        if (bChanged)
        {
            bAnyChanged = true;
            PropagatePropertyChange(Props[PropIndex].Name, SelectedActors);
            EditorEngine->CommitTrackedSceneChange();
        }
        else
        {
            EditorEngine->CancelTrackedSceneChange();
        }
    }

    ImGui::PopStyleColor(7);
}

void FEditorPropertyWidget::PropagatePropertyChange(const FString &PropName, const TArray<AActor *> &SelectedActors)
{
    if (!SelectedComponent || SelectedActors.size() < 2)
        return;

    UClass *CompClass = SelectedComponent->GetClass();
    AActor *PrimaryActor = SelectedActors[0];

    // Primary 컴포넌트에서 변경된 프로퍼티의 값 포인터 찾기
    TArray<FPropertyDescriptor> SrcProps;
    SelectedComponent->GetEditableProperties(SrcProps);

    const FPropertyDescriptor *SrcProp = nullptr;
    for (const auto &P : SrcProps)
    {
        if (P.Name == PropName)
        {
            SrcProp = &P;
            break;
        }
    }
    if (!SrcProp)
        return;

    for (AActor *Actor : SelectedActors)
    {
        if (!Actor || Actor == PrimaryActor)
            continue;

        for (UActorComponent *Comp : Actor->GetComponents())
        {
            if (!Comp || Comp->GetClass() != CompClass)
                continue;

            TArray<FPropertyDescriptor> DstProps;
            Comp->GetEditableProperties(DstProps);

            for (const auto &DstProp : DstProps)
            {
                if (DstProp.Name != PropName || DstProp.Type != SrcProp->Type)
                    continue;

                size_t Size = 0;
                switch (DstProp.Type)
                {
                case EPropertyType::Bool:
                    Size = sizeof(bool);
                    break;
                case EPropertyType::ByteBool:
                    Size = sizeof(uint8);
                    break;
                case EPropertyType::Int:
                    Size = sizeof(int32);
                    break;
                case EPropertyType::Float:
                    Size = sizeof(float);
                    break;
                case EPropertyType::Vec3:
                case EPropertyType::Rotator:
                    Size = sizeof(float) * 3;
                    break;
                case EPropertyType::Vec4:
                case EPropertyType::Color4:
                    Size = sizeof(float) * 4;
                    break;
                case EPropertyType::String:
                case EPropertyType::SceneComponentRef:
                case EPropertyType::StaticMeshRef:
                    *static_cast<FString *>(DstProp.ValuePtr) = *static_cast<FString *>(SrcProp->ValuePtr);
                    break;
                case EPropertyType::Name:
                    *static_cast<FName *>(DstProp.ValuePtr) = *static_cast<FName *>(SrcProp->ValuePtr);
                    break;
                case EPropertyType::MaterialSlot:
                    *static_cast<FMaterialSlot *>(DstProp.ValuePtr) = *static_cast<FMaterialSlot *>(SrcProp->ValuePtr);
                    break;
                case EPropertyType::TextureSlot:
                    *static_cast<FTextureSlot *>(DstProp.ValuePtr) = *static_cast<FTextureSlot *>(SrcProp->ValuePtr);
                    break;
                case EPropertyType::Enum:
                    Size = sizeof(int32);
                    break;
                case EPropertyType::Vec3Array:
                    *static_cast<TArray<FVector> *>(DstProp.ValuePtr) = *static_cast<TArray<FVector> *>(SrcProp->ValuePtr);
                    break;
                }
                if (Size > 0)
                    memcpy(DstProp.ValuePtr, SrcProp->ValuePtr, Size);

                Comp->PostEditProperty(PropName.c_str());
                break;
            }
            break; // 같은 타입의 첫 번째 컴포넌트에만 전파
        }
    }
}

bool FEditorPropertyWidget::RenderPropertyWidget(TArray<FPropertyDescriptor> &Props, int32 &Index)
{
    ImGui::PushID(Index);
    FPropertyDescriptor &Prop = Props[Index];
    const FString        DisplayName = GetDisplayPropertyLabel(Prop.Name);
    const char          *Label = DisplayName.c_str();
    bool                 bChanged = false;

    switch (Prop.Type)
    {
    case EPropertyType::Bool:
    {
        bool *Val = static_cast<bool *>(Prop.ValuePtr);
        bChanged = ImGui::Checkbox(Label, Val);
        break;
    }
    case EPropertyType::ByteBool:
    {
        uint8 *Val = static_cast<uint8 *>(Prop.ValuePtr);
        bool   bVal = (*Val != 0);
        if (ImGui::Checkbox(Label, &bVal))
        {
            *Val = bVal ? 1 : 0;
            bChanged = true;
        }
        break;
    }
    case EPropertyType::Int:
    {
        int32 *Val = static_cast<int32 *>(Prop.ValuePtr);
        PushDetailsFieldStyle();
        if (Prop.Min != 0.0f || Prop.Max != 0.0f)
            bChanged = ImGui::DragInt(Label, Val, Prop.Speed, (int32)Prop.Min, (int32)Prop.Max);
        else
            bChanged = ImGui::DragInt(Label, Val, Prop.Speed);
        PopDetailsFieldStyle();
        break;
    }
    case EPropertyType::Float:
    {
        float *Val = static_cast<float *>(Prop.ValuePtr);
        PushDetailsFieldStyle();
        if (Prop.Min != 0.0f || Prop.Max != 0.0f)
            bChanged = ImGui::DragFloat(Label, Val, Prop.Speed, Prop.Min, Prop.Max, "%.4f");
        else
            bChanged = ImGui::DragFloat(Label, Val, Prop.Speed);
        PopDetailsFieldStyle();
        break;
    }
    case EPropertyType::Vec3:
    {
        float *Val = static_cast<float *>(Prop.ValuePtr);
        bChanged = DrawColoredFloat3(Label, Val, Prop.Speed);
        break;
    }
    case EPropertyType::Rotator:
    {
        // FRotator 메모리 레이아웃 [Pitch,Yaw,Roll] → UI X=Roll(X축), Y=Pitch(Y축), Z=Yaw(Z축)
        FRotator *Rot = static_cast<FRotator *>(Prop.ValuePtr);
        float     RotXYZ[3] = {Rot->Roll, Rot->Pitch, Rot->Yaw};
        bChanged = DrawColoredFloat3(Label, RotXYZ, Prop.Speed);
        if (bChanged)
        {
            Rot->Roll = RotXYZ[0];
            Rot->Pitch = RotXYZ[1];
            Rot->Yaw = RotXYZ[2];
            if (SelectedComponent && SelectedComponent->IsA<USceneComponent>())
            {
                static_cast<USceneComponent *>(SelectedComponent)->ApplyCachedEditRotator();
            }
        }
        break;
    }
    case EPropertyType::Vec4:
    {
        float *Val = static_cast<float *>(Prop.ValuePtr);
        PushDetailsFieldStyle();
        bChanged = DrawColoredFloat4(Label, Val, Prop.Speed);
        PopDetailsFieldStyle();
        break;
    }
    case EPropertyType::Color4:
    {
        float *Val = static_cast<float *>(Prop.ValuePtr);
        bChanged = ImGui::ColorEdit4(Label, Val);
        break;
    }
    case EPropertyType::String:
    {
        FString *Val = static_cast<FString *>(Prop.ValuePtr);
        PushDetailsFieldStyle();
        const bool bIsScriptPath = (Prop.Name == "ScriptPath") && Cast<UScriptComponent>(SelectedComponent);
        if (bIsScriptPath)
        {
            if (ScriptPathEditComponent != SelectedComponent)
            {
                ScriptPathEditComponent = SelectedComponent;
                bScriptPathEditActive = false;
                strncpy_s(ScriptPathEditBuffer, sizeof(ScriptPathEditBuffer), Val->c_str(), _TRUNCATE);
            }

            if (!bScriptPathEditActive && FString(ScriptPathEditBuffer) != *Val)
            {
                strncpy_s(ScriptPathEditBuffer, sizeof(ScriptPathEditBuffer), Val->c_str(), _TRUNCATE);
            }

            auto CommitScriptPathEdit = [&]()
            {
                const FString NewValue = ScriptPathEditBuffer;
                if (*Val != NewValue)
                {
                    *Val = NewValue;
                    bChanged = true;
                }
            };

            if (ImGui::InputText(Label, ScriptPathEditBuffer, sizeof(ScriptPathEditBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                CommitScriptPathEdit();
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                CommitScriptPathEdit();
            }
            bScriptPathEditActive = ImGui::IsItemActive();
        }
        else
        {
            char Buf[256];
            strncpy_s(Buf, sizeof(Buf), Val->c_str(), _TRUNCATE);
            if (ImGui::InputText(Label, Buf, sizeof(Buf)))
            {
                *Val = Buf;
                bChanged = true;
            }
        }
        PopDetailsFieldStyle();
        break;
    }
    case EPropertyType::SceneComponentRef:
    {
        FString            *Val = static_cast<FString *>(Prop.ValuePtr);
        UMovementComponent *MovementComp = SelectedComponent ? Cast<UMovementComponent>(SelectedComponent) : nullptr;
        FString             Preview = MovementComp ? MovementComp->GetUpdatedComponentDisplayName() : FString("None");

        if (ImGui::BeginCombo(Label, Preview.c_str()))
        {
            bool bSelectedAuto = Val->empty();
            if (ImGui::Selectable("Auto (Root)", bSelectedAuto))
            {
                Val->clear();
                bChanged = true;
            }
            if (bSelectedAuto)
            {
                ImGui::SetItemDefaultFocus();
            }

            if (MovementComp)
            {
                for (USceneComponent *Candidate : MovementComp->GetOwnerSceneComponents())
                {
                    if (!Candidate)
                    {
                        continue;
                    }

                    FString CandidatePath = MovementComp->BuildUpdatedComponentPath(Candidate);
                    FString CandidateName = Candidate->GetFName().ToString();
                    if (CandidateName.empty())
                    {
                        CandidateName = Candidate->GetClass()->GetName();
                    }
                    if (!CandidatePath.empty())
                    {
                        CandidateName += " (" + CandidatePath + ")";
                    }

                    bool bSelected = (*Val == CandidatePath);
                    if (ImGui::Selectable(CandidateName.c_str(), bSelected))
                    {
                        *Val = CandidatePath;
                        bChanged = true;
                    }
                    if (bSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }

            ImGui::EndCombo();
        }
        break;
    }
    case EPropertyType::StaticMeshRef:
    {
        FString *Val = static_cast<FString *>(Prop.ValuePtr);

        FString Preview = Val->empty() ? "None" : GetStemFromPath(*Val);
        if (*Val == "None")
            Preview = "None";

        ImGui::Text("%s", Label);
        ImGui::SameLine(120);

        float ButtonWidth = ImGui::CalcTextSize("Import OBJ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float Spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(-(ButtonWidth + Spacing));

        if (ImGui::BeginCombo("##Mesh", Preview.c_str()))
        {
            bool bSelectedNone = (*Val == "None");
            if (ImGui::Selectable("None", bSelectedNone))
            {
                *Val = "None";
                bChanged = true;
            }
            if (bSelectedNone)
                ImGui::SetItemDefaultFocus();

            const TArray<FMeshAssetListItem> &MeshFiles = FObjManager::GetAvailableMeshFiles();
            for (const FMeshAssetListItem &Item : MeshFiles)
            {
                bool bSelected = (*Val == Item.FullPath);
                if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                {
                    *Val = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // .obj 임포트 버튼
        ImGui::SameLine();

        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ButtonWidth);
        if (ImGui::Button("Import OBJ"))
        {
            FString ObjPath = OpenObjFileDialog();
            if (!ObjPath.empty())
            {
                ID3D11Device *Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
                UStaticMesh  *Loaded = FObjManager::LoadObjStaticMesh(ObjPath, Device);
                if (Loaded)
                {
                    *Val = FObjManager::GetBinaryFilePath(ObjPath);
                    bChanged = true;
                }
            }
        }
        break;
    }
    case EPropertyType::MaterialSlot:
    {
        FMaterialSlot *Slot = static_cast<FMaterialSlot *>(Prop.ValuePtr);
        int32          ElemIdx = (strncmp(Prop.Name.c_str(), "Element ", 8) == 0) ? atoi(&Prop.Name[8]) : -1;

        FString SlotName = "None";
        if (ElemIdx != -1 && SelectedComponent && SelectedComponent->IsA<UStaticMeshComponent>())
        {
            UStaticMeshComponent *SMC = static_cast<UStaticMeshComponent *>(SelectedComponent);
            if (SMC->GetStaticMesh() && ElemIdx < (int32)SMC->GetStaticMesh()->GetStaticMaterials().size())
                SlotName = SMC->GetStaticMesh()->GetStaticMaterials()[ElemIdx].MaterialSlotName;
        }

        // 좌측: Element 인덱스 + 슬롯 이름
        ImGui::BeginGroup();
        ImGui::Text("Element %d", ElemIdx);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextUnformatted(SlotName.c_str());
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", SlotName.c_str());
        ImGui::EndGroup();

        ImGui::SameLine(120);

        // 우측: Material 콤보
        ImGui::BeginGroup();
        ImGui::PushID(Prop.Name.c_str());
        constexpr float PreviewImageSize = 26.0f;
        constexpr float PreviewSpacing = 6.0f;
        UMaterial *CurrentMaterial = (Slot->Path.empty() || Slot->Path == "None") ? nullptr : FMaterialManager::Get().GetOrCreateMaterial(Slot->Path);
        UTexture2D *CurrentPreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(CurrentMaterial);
        const float ReservedPreviewWidth = (CurrentPreviewTexture && CurrentPreviewTexture->GetSRV()) ? (PreviewImageSize + PreviewSpacing) : 0.0f;
        float       ComboWidth = ImGui::GetContentRegionAvail().x - ReservedPreviewWidth;
        if (ComboWidth < 120.0f)
        {
            ComboWidth = 120.0f;
        }
        ImGui::SetNextItemWidth(ComboWidth);

        FString Preview = MakeAssetPreviewLabel(Slot->Path);
        if (ImGui::BeginCombo("##MatCombo", Preview.c_str()))
        {
            // "None" 선택지 기본 제공
            bool bSelectedNone = (Slot->Path == "None" || Slot->Path.empty());
            if (ImGui::Selectable("None", bSelectedNone))
            {
                Slot->Path = "None";
                bChanged = true;
            }
            if (bSelectedNone)
                ImGui::SetItemDefaultFocus();

            // TObjectIterator 대신 FMaterialManager 파일 목록 스캔 데이터 사용
            const TArray<FMaterialAssetListItem> &MatFiles = FMaterialManager::Get().GetAvailableMaterialFiles();
            for (const FMaterialAssetListItem &Item : MatFiles)
            {
                bool        bSelected = (Slot->Path == Item.FullPath);
                UTexture2D *PreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(Item.FullPath);
                if (PreviewTexture && PreviewTexture->GetSRV())
                {
                    ImGui::Image(PreviewTexture->GetSRV(), ImVec2(24.0f, 24.0f));
                    ImGui::SameLine();
                }
                if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                {
                    Slot->Path = Item.FullPath; // 데이터는 전체 경로로 저장
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
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
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MaterialContentItem"))
            {
                FContentItem ContentItem = *reinterpret_cast<const FContentItem *>(payload->Data);
                Slot->Path = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
                bChanged = true;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
        ImGui::EndGroup();
        break;
    }
    case EPropertyType::TextureSlot:
    {
        FTextureSlot *Slot = static_cast<FTextureSlot *>(Prop.ValuePtr);
        FString       Preview = MakeAssetPreviewLabel(Slot->Path);

        if (ImGui::BeginCombo(Label, Preview.c_str()))
        {
            bool bSelectedNone = (Slot->Path == "None" || Slot->Path.empty());
            if (ImGui::Selectable("None", bSelectedNone))
            {
                Slot->Path = "None";
                bChanged = true;
            }
            if (bSelectedNone)
            {
                ImGui::SetItemDefaultFocus();
            }

            const TArray<FTextureAssetListItem> &TextureFiles = UTexture2D::GetAvailableTextureFiles();
            for (const FTextureAssetListItem &Item : TextureFiles)
            {
                bool        bSelected = (Slot->Path == Item.FullPath);
                UTexture2D *PreviewTexture = UTexture2D::LoadFromCached(Item.FullPath);
                if (PreviewTexture && PreviewTexture->GetSRV())
                {
                    ImGui::Image(PreviewTexture->GetSRV(), ImVec2(24.0f, 24.0f));
                    ImGui::SameLine();
                }

                if (ImGui::Selectable(Item.DisplayName.c_str(), bSelected))
                {
                    Slot->Path = Item.FullPath;
                    bChanged = true;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("PNGElement"))
            {
                FContentItem ContentItem = *reinterpret_cast<const FContentItem *>(payload->Data);
                Slot->Path = FPaths::ToUtf8(ContentItem.Path.lexically_relative(FPaths::RootDir()).generic_wstring());
                bChanged = true;
            }
            ImGui::EndDragDropTarget();
        }
        break;
    }
    case EPropertyType::Name:
    {
        FName  *Val = static_cast<FName *>(Prop.ValuePtr);
        FString Current = Val->ToString();

        // 리소스 키와 매칭되는 프로퍼티면 콤보 박스로 렌더링
        TArray<FString> Names;
        if (strcmp(Prop.Name.c_str(), "Font") == 0)
            Names = FResourceManager::Get().GetFontNames();
        else if (strcmp(Prop.Name.c_str(), "Particle") == 0)
            Names = FResourceManager::Get().GetParticleNames();
        else if (strcmp(Prop.Name.c_str(), "Texture") == 0)
            Names = FResourceManager::Get().GetTextureNames();

        if (!Names.empty())
        {
            if (ImGui::BeginCombo(Label, Current.c_str()))
            {
                for (const auto &Name : Names)
                {
                    bool bSelected = (Current == Name);
                    if (ImGui::Selectable(Name.c_str(), bSelected))
                    {
                        *Val = FName(Name);
                        bChanged = true;
                    }
                    if (bSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            char Buf[256];
            strncpy_s(Buf, sizeof(Buf), Current.c_str(), _TRUNCATE);
            if (ImGui::InputText(Label, Buf, sizeof(Buf)))
            {
                *Val = FName(Buf);
                bChanged = true;
            }
        }
        break;
    }
    case EPropertyType::Enum:
    {
        if (!Prop.EnumNames || Prop.EnumCount == 0)
            break;
        int32      *Val = static_cast<int32 *>(Prop.ValuePtr);
        const char *Preview = ((uint32)*Val < Prop.EnumCount) ? Prop.EnumNames[*Val] : "Unknown";
        if (ImGui::BeginCombo(Label, Preview))
        {
            for (uint32 i = 0; i < Prop.EnumCount; ++i)
            {
                bool bSelected = (*Val == (int32)i);
                if (ImGui::Selectable(Prop.EnumNames[i], bSelected))
                {
                    *Val = (int32)i;
                    bChanged = true;
                }
                if (bSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        break;
    }
    case EPropertyType::Vec3Array:
    {
        TArray<FVector> *Arr = static_cast<TArray<FVector> *>(Prop.ValuePtr);

        ImGui::TextUnformatted(Label);

        int32 RemoveIdx = -1;
        for (int32 i = 0; i < (int32)Arr->size(); ++i)
        {
            ImGui::PushID(i);
            char PointLabel[16];
            snprintf(PointLabel, sizeof(PointLabel), "[%d]", i);
            float Point[3] = {(*Arr)[i].X, (*Arr)[i].Y, (*Arr)[i].Z};
            if (DrawColoredFloat3(PointLabel, Point, 1.0f))
            {
                (*Arr)[i].X = Point[0];
                (*Arr)[i].Y = Point[1];
                (*Arr)[i].Z = Point[2];
                bChanged = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
                RemoveIdx = i;
            ImGui::PopID();
        }
        if (RemoveIdx >= 0)
        {
            Arr->erase(Arr->begin() + RemoveIdx);
            bChanged = true;
        }
        if (ImGui::Button("+ Add Point"))
        {
            Arr->push_back(FVector(0.0f, 0.0f, 0.0f));
            bChanged = true;
        }
        break;
    }
    }

    if (bChanged && SelectedComponent)
    {
        SelectedComponent->PostEditProperty(Prop.Name.c_str());
    }

    ImGui::PopID();
    return bChanged;
}
