#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "Platform/Paths.h"
#include "Render/Pipeline/Renderer.h"
#include "Resource/ResourceManager.h"
#include "WICTextureLoader.h"
#include "ImGui/imgui.h"

#include <algorithm>

namespace
{
    enum class EPreviewToolbarIcon : int32
    {
        AddActor = 0,
        Translate,
        Rotate,
        Scale,
        WorldSpace,
        LocalSpace,
        TranslateSnap,
        CameraSettings,
        ShowFlag,
        ViewModeLit,
        ViewModeUnlit,
        ViewModeWireframe,
        ViewportPerspective,
        ViewportTop,
        ViewportFront,
        ViewportRight,
        Menu,
        Count
    };

    const char *GetPreviewToolbarIconResourceKey(EPreviewToolbarIcon Icon)
    {
        switch (Icon)
        {
        case EPreviewToolbarIcon::AddActor: return "Editor.ToolIcon.AddActor";
        case EPreviewToolbarIcon::Translate: return "Editor.ToolIcon.Translate";
        case EPreviewToolbarIcon::Rotate: return "Editor.ToolIcon.Rotate";
        case EPreviewToolbarIcon::Scale: return "Editor.ToolIcon.Scale";
        case EPreviewToolbarIcon::WorldSpace: return "Editor.ToolIcon.WorldSpace";
        case EPreviewToolbarIcon::LocalSpace: return "Editor.ToolIcon.LocalSpace";
        case EPreviewToolbarIcon::TranslateSnap: return "Editor.ToolIcon.TranslateSnap";
        case EPreviewToolbarIcon::CameraSettings: return "Editor.ToolIcon.Camera";
        case EPreviewToolbarIcon::ShowFlag: return "Editor.ToolIcon.ShowFlag";
        case EPreviewToolbarIcon::ViewModeLit: return "Editor.ToolIcon.ViewMode.Lit";
        case EPreviewToolbarIcon::ViewModeUnlit: return "Editor.ToolIcon.ViewMode.Unlit";
        case EPreviewToolbarIcon::ViewModeWireframe: return "Editor.ToolIcon.ViewMode.Wireframe";
        case EPreviewToolbarIcon::ViewportPerspective: return "Editor.ToolIcon.Viewport.Perspective";
        case EPreviewToolbarIcon::ViewportTop: return "Editor.ToolIcon.Viewport.Top";
        case EPreviewToolbarIcon::ViewportFront: return "Editor.ToolIcon.Viewport.Front";
        case EPreviewToolbarIcon::ViewportRight: return "Editor.ToolIcon.Viewport.Right";
        case EPreviewToolbarIcon::Menu: return "Editor.ToolIcon.Menu";
        default: return "";
        }
    }

    ID3D11ShaderResourceView **GetPreviewToolbarIconTable()
    {
        static ID3D11ShaderResourceView *Icons[static_cast<int32>(EPreviewToolbarIcon::Count)] = {};
        return Icons;
    }

    bool bPreviewToolbarIconsLoaded = false;

    void EnsurePreviewToolbarIconsLoaded(FRenderer *Renderer)
    {
        if (bPreviewToolbarIconsLoaded || !Renderer)
        {
            return;
        }

        ID3D11Device *Device = Renderer->GetFD3DDevice().GetDevice();
        if (!Device)
        {
            return;
        }

        ID3D11ShaderResourceView **Icons = GetPreviewToolbarIconTable();
        for (int32 i = 0; i < static_cast<int32>(EPreviewToolbarIcon::Count); ++i)
        {
            const FString Path = FResourceManager::Get().ResolvePath(FName(GetPreviewToolbarIconResourceKey(static_cast<EPreviewToolbarIcon>(i))));
            DirectX::CreateWICTextureFromFile(Device, FPaths::ToWide(Path).c_str(), nullptr, &Icons[i]);
        }

        bPreviewToolbarIconsLoaded = true;
    }

    ImVec2 GetIconSize(EPreviewToolbarIcon Icon, float FallbackSize, float MaxIconSize)
    {
        ID3D11ShaderResourceView *SRV = GetPreviewToolbarIconTable()[static_cast<int32>(Icon)];
        if (!SRV)
        {
            return ImVec2(FallbackSize, FallbackSize);
        }

        ID3D11Resource *Resource = nullptr;
        SRV->GetResource(&Resource);
        if (!Resource)
        {
            return ImVec2(FallbackSize, FallbackSize);
        }

        ImVec2 Size(FallbackSize, FallbackSize);
        D3D11_RESOURCE_DIMENSION Dimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        Resource->GetType(&Dimension);
        if (Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
        {
            ID3D11Texture2D *Texture = static_cast<ID3D11Texture2D *>(Resource);
            D3D11_TEXTURE2D_DESC Desc{};
            Texture->GetDesc(&Desc);
            Size = ImVec2(static_cast<float>(Desc.Width), static_cast<float>(Desc.Height));
        }
        Resource->Release();

        if (Size.x > MaxIconSize || Size.y > MaxIconSize)
        {
            const float Scale = (Size.x > Size.y) ? (MaxIconSize / Size.x) : (MaxIconSize / Size.y);
            Size.x *= Scale;
            Size.y *= Scale;
        }
        return Size;
    }

    void ShowTooltip(const char *Text)
    {
        if (Text && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("%s", Text);
        }
    }

    bool DrawIconButton(const char *Id, EPreviewToolbarIcon Icon, const char *FallbackLabel, const char *Tooltip,
                        bool bSelected = false)
    {
        constexpr float FallbackSize = 14.0f;
        constexpr float MaxIconSize = 16.0f;
        ID3D11ShaderResourceView *SRV = GetPreviewToolbarIconTable()[static_cast<int32>(Icon)];
        bool bClicked = false;
        if (SRV)
        {
            const ImU32 Tint = bSelected ? IM_COL32(70, 150, 255, 255) : IM_COL32_WHITE;
            bClicked = ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(SRV), GetIconSize(Icon, FallbackSize, MaxIconSize),
                                          ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec4(0, 0, 0, 0),
                                          ImGui::ColorConvertU32ToFloat4(Tint));
        }
        else
        {
            bClicked = ImGui::Button(FallbackLabel);
        }
        ShowTooltip(Tooltip);
        return bClicked;
    }

    bool DrawIconLabelButton(const char *Id, EPreviewToolbarIcon Icon, const char *Label, float Width, const char *Tooltip)
    {
        constexpr float Height = 26.0f;
        constexpr float FallbackSize = 14.0f;
        constexpr float MaxIconSize = 16.0f;
        const bool bClicked = ImGui::Button(Id, ImVec2(Width, Height));

        ImDrawList *DrawList = ImGui::GetWindowDrawList();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const ImVec2 IconSize = GetIconSize(Icon, FallbackSize, MaxIconSize);
        const float IconX = Min.x + 7.0f;
        const float IconY = Min.y + ((Max.y - Min.y) - IconSize.y) * 0.5f;
        if (ID3D11ShaderResourceView *SRV = GetPreviewToolbarIconTable()[static_cast<int32>(Icon)])
        {
            DrawList->AddImage(reinterpret_cast<ImTextureID>(SRV), ImVec2(IconX, IconY), ImVec2(IconX + IconSize.x, IconY + IconSize.y));
        }
        DrawList->AddText(ImVec2(IconX + IconSize.x + 5.0f, Min.y + 5.0f), ImGui::GetColorU32(ImGuiCol_Text), Label);
        DrawList->AddText(ImVec2(Max.x - 14.0f, Min.y + 5.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "v");
        ShowTooltip(Tooltip);
        return bClicked;
    }
}

void FSkeletalMeshEditorToolbar::RenderViewportToolbar(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, FRenderer *Renderer)
{
    EnsurePreviewToolbarIconsLoaded(Renderer);

    const bool bHasMesh = Mesh != nullptr;
    constexpr float ButtonSpacing = 6.0f;

    ImGui::BeginDisabled(!bHasMesh);

    if (DrawIconButton("##PreviewAddActor", EPreviewToolbarIcon::AddActor, "Add", "Add / Place"))
    {
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconButton("##PreviewTranslate", EPreviewToolbarIcon::Translate, "T", "Translate",
                       State.bEnablePoseEditMode && State.GizmoMode == EGizmoMode::Translate))
    {
        State.bEnablePoseEditMode = true;
        State.GizmoMode = EGizmoMode::Translate;
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconButton("##PreviewRotate", EPreviewToolbarIcon::Rotate, "R", "Rotate",
                       State.bEnablePoseEditMode && State.GizmoMode == EGizmoMode::Rotate))
    {
        State.bEnablePoseEditMode = true;
        State.GizmoMode = EGizmoMode::Rotate;
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconButton("##PreviewScale", EPreviewToolbarIcon::Scale, "S", "Scale",
                       State.bEnablePoseEditMode && State.GizmoMode == EGizmoMode::Scale))
    {
        State.bEnablePoseEditMode = true;
        State.GizmoMode = EGizmoMode::Scale;
    }

    ImGui::SameLine(0.0f, ButtonSpacing + 4.0f);
    if (DrawIconButton("##PreviewWorldSpace", EPreviewToolbarIcon::WorldSpace, "World", "World Space",
                       State.GizmoSpace == EGizmoSpace::World))
    {
        State.GizmoSpace = EGizmoSpace::World;
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconButton("##PreviewLocalSpace", EPreviewToolbarIcon::LocalSpace, "Local", "Local Space",
                       State.GizmoSpace == EGizmoSpace::Local))
    {
        State.GizmoSpace = EGizmoSpace::Local;
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconButton("##PreviewSnap", EPreviewToolbarIcon::TranslateSnap, "Snap", "Snap Settings"))
    {
        ImGui::OpenPopup("##PreviewSnapSettings");
    }
    if (ImGui::BeginPopup("##PreviewSnapSettings"))
    {
        ImGui::TextDisabled("Preview snap settings are not used yet.");
        ImGui::EndPopup();
    }

    const float RightGroupWidth = 350.0f;
    const float RightStartX = ImGui::GetWindowWidth() - RightGroupWidth;
    if (RightStartX > ImGui::GetCursorPosX() + 12.0f)
    {
        ImGui::SameLine(RightStartX);
    }
    else
    {
        ImGui::SameLine(0.0f, ButtonSpacing);
    }

    if (DrawIconLabelButton("##PreviewCamera", EPreviewToolbarIcon::CameraSettings, "", 36.0f, "Camera Settings"))
    {
        ImGui::OpenPopup("##PreviewCameraSettings");
    }
    if (ImGui::BeginPopup("##PreviewCameraSettings"))
    {
        if (ImGui::MenuItem("Frame Selected"))
        {
            State.bFramePreviewRequested = true;
        }
        ImGui::MenuItem("Perspective", nullptr, true, false);
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconLabelButton("##PreviewViewportType", EPreviewToolbarIcon::ViewportPerspective, "Perspective", 116.0f, "Viewport Type"))
    {
        ImGui::OpenPopup("##PreviewViewportTypePopup");
    }
    if (ImGui::BeginPopup("##PreviewViewportTypePopup"))
    {
        ImGui::MenuItem("Perspective", nullptr, true, false);
        ImGui::MenuItem("Top", nullptr, false, false);
        ImGui::MenuItem("Front", nullptr, false, false);
        ImGui::MenuItem("Right", nullptr, false, false);
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconLabelButton("##PreviewViewMode", EPreviewToolbarIcon::ViewModeLit, "Lit", 72.0f, "View Mode"))
    {
        ImGui::OpenPopup("##PreviewViewModePopup");
    }
    if (ImGui::BeginPopup("##PreviewViewModePopup"))
    {
        ImGui::MenuItem("Lit", nullptr, true, false);
        ImGui::MenuItem("Unlit", nullptr, false, false);
        ImGui::MenuItem("Wireframe", nullptr, false, false);
        ImGui::MenuItem("World Normal", nullptr, false, false);
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconLabelButton("##PreviewShow", EPreviewToolbarIcon::ShowFlag, "", 36.0f, "Show"))
    {
        ImGui::OpenPopup("##PreviewShowFlags");
    }
    if (ImGui::BeginPopup("##PreviewShowFlags"))
    {
        ImGui::MenuItem("Grid", nullptr, &State.bShowGrid);
        ImGui::MenuItem("Bones", nullptr, &State.bShowBones);
        ImGui::MenuItem("Mesh Stats", nullptr, &State.bShowMeshStatsOverlay);
        ImGui::Separator();
        ImGui::MenuItem("Reference Pose", nullptr, &State.bShowReferencePose);
        ImGui::EndPopup();
    }

    ImGui::SameLine(0.0f, ButtonSpacing);
    if (DrawIconLabelButton("##PreviewLayout", EPreviewToolbarIcon::Menu, "", 36.0f, "Viewport Layout"))
    {
        ImGui::OpenPopup("##PreviewLayoutPopup");
    }
    if (ImGui::BeginPopup("##PreviewLayoutPopup"))
    {
        ImGui::MenuItem("One Pane", nullptr, true, false);
        ImGui::MenuItem("Two Panes", nullptr, false, false);
        ImGui::MenuItem("Four Panes", nullptr, false, false);
        ImGui::EndPopup();
    }

    ImGui::EndDisabled();
}
