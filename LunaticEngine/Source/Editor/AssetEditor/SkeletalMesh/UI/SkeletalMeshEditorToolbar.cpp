#include "AssetEditor/SkeletalMesh/UI/SkeletalMeshEditorToolbar.h"

#include "Common/UI/Viewport/EditorViewportToolbar.h"

#include "Engine/Mesh/SkeletalMesh.h"
#include "Render/Pipeline/Renderer.h"
#include "ImGui/imgui.h"

#include <cstdio>

namespace
{
    FEditorViewportToolbar::EToolMode ToCommonToolMode(EGizmoMode Mode)
    {
        switch (Mode)
        {
        case EGizmoMode::Rotate: return FEditorViewportToolbar::EToolMode::Rotate;
        case EGizmoMode::Scale: return FEditorViewportToolbar::EToolMode::Scale;
        case EGizmoMode::Translate:
        default: return FEditorViewportToolbar::EToolMode::Translate;
        }
    }

    EGizmoMode ToGizmoMode(FEditorViewportToolbar::EToolMode Mode)
    {
        switch (Mode)
        {
        case FEditorViewportToolbar::EToolMode::Rotate: return EGizmoMode::Rotate;
        case FEditorViewportToolbar::EToolMode::Scale: return EGizmoMode::Scale;
        case FEditorViewportToolbar::EToolMode::Translate:
        default: return EGizmoMode::Translate;
        }
    }

    const char *GetViewportTypeLabel(ESkeletalMeshPreviewViewportType Type)
    {
        switch (Type)
        {
        case ESkeletalMeshPreviewViewportType::Top: return "Top";
        case ESkeletalMeshPreviewViewportType::Bottom: return "Bottom";
        case ESkeletalMeshPreviewViewportType::Left: return "Left";
        case ESkeletalMeshPreviewViewportType::Right: return "Right";
        case ESkeletalMeshPreviewViewportType::Front: return "Front";
        case ESkeletalMeshPreviewViewportType::Back: return "Back";
        case ESkeletalMeshPreviewViewportType::FreeOrtho: return "Free Ortho";
        case ESkeletalMeshPreviewViewportType::Perspective:
        default: return "Perspective";
        }
    }

    FEditorViewportToolbar::EIcon GetViewportTypeIcon(ESkeletalMeshPreviewViewportType Type)
    {
        switch (Type)
        {
        case ESkeletalMeshPreviewViewportType::Top: return FEditorViewportToolbar::EIcon::ViewportTop;
        case ESkeletalMeshPreviewViewportType::Bottom: return FEditorViewportToolbar::EIcon::ViewportBottom;
        case ESkeletalMeshPreviewViewportType::Left: return FEditorViewportToolbar::EIcon::ViewportLeft;
        case ESkeletalMeshPreviewViewportType::Right: return FEditorViewportToolbar::EIcon::ViewportRight;
        case ESkeletalMeshPreviewViewportType::Front: return FEditorViewportToolbar::EIcon::ViewportFront;
        case ESkeletalMeshPreviewViewportType::Back: return FEditorViewportToolbar::EIcon::ViewportBack;
        case ESkeletalMeshPreviewViewportType::FreeOrtho: return FEditorViewportToolbar::EIcon::ViewportFreeOrtho;
        case ESkeletalMeshPreviewViewportType::Perspective:
        default: return FEditorViewportToolbar::EIcon::ViewportPerspective;
        }
    }

    const char *GetViewModeLabel(ESkeletalMeshPreviewViewMode Mode)
    {
        // LevelViewportLayout.cpp의 ViewModeNames와 동일하게 Lit 계열은 toolbar label을 Lit으로 보이게 둔다.
        switch (Mode)
        {
        case ESkeletalMeshPreviewViewMode::Unlit: return "Unlit";
        case ESkeletalMeshPreviewViewMode::Wireframe: return "Wireframe";
        case ESkeletalMeshPreviewViewMode::SceneDepth: return "Scene Depth";
        case ESkeletalMeshPreviewViewMode::WorldNormal: return "World Normal";
        case ESkeletalMeshPreviewViewMode::LightCulling: return "Light Culling";
        case ESkeletalMeshPreviewViewMode::LitGouraud:
        case ESkeletalMeshPreviewViewMode::LitLambert:
        case ESkeletalMeshPreviewViewMode::Lit:
        default: return "Lit";
        }
    }

    FEditorViewportToolbar::EIcon GetViewModeIcon(ESkeletalMeshPreviewViewMode Mode)
    {
        switch (Mode)
        {
        case ESkeletalMeshPreviewViewMode::Unlit: return FEditorViewportToolbar::EIcon::ViewModeUnlit;
        case ESkeletalMeshPreviewViewMode::Wireframe: return FEditorViewportToolbar::EIcon::ViewModeWireframe;
        case ESkeletalMeshPreviewViewMode::SceneDepth: return FEditorViewportToolbar::EIcon::ViewModeSceneDepth;
        case ESkeletalMeshPreviewViewMode::WorldNormal: return FEditorViewportToolbar::EIcon::ViewModeWorldNormal;
        case ESkeletalMeshPreviewViewMode::LightCulling: return FEditorViewportToolbar::EIcon::ViewModeLightCulling;
        case ESkeletalMeshPreviewViewMode::LitGouraud:
        case ESkeletalMeshPreviewViewMode::LitLambert:
        case ESkeletalMeshPreviewViewMode::Lit:
        default: return FEditorViewportToolbar::EIcon::ViewModeLit;
        }
    }

    void DrawViewportTypeOption(FSkeletalMeshEditorState &State, const char *Label, ESkeletalMeshPreviewViewportType Type)
    {
        if (FEditorViewportToolbar::DrawIconSelectable(Label, GetViewportTypeIcon(Type), Label, State.PreviewViewportType == Type, 220.0f))
        {
            State.PreviewViewportType = Type;
            ImGui::CloseCurrentPopup();
        }
    }

    void DrawViewModeOption(FSkeletalMeshEditorState &State, const char *Label, ESkeletalMeshPreviewViewMode Mode)
    {
        if (FEditorViewportToolbar::DrawIconSelectable(Label, GetViewModeIcon(Mode), Label, State.PreviewViewMode == Mode, 260.0f))
        {
            State.PreviewViewMode = Mode;
            ImGui::CloseCurrentPopup();
        }
    }

    void DrawSnapValueRow(const char *Label, bool &bEnabled, float &Value, const float *Values, int32 Count, const char *Format)
    {
        ImGui::Checkbox(Label, &bEnabled);
        ImGui::BeginDisabled(!bEnabled);
        ImGui::Indent(12.0f);
        for (int32 i = 0; i < Count; ++i)
        {
            ImGui::PushID(i);
            char ButtonLabel[32];
            snprintf(ButtonLabel, sizeof(ButtonLabel), Format, Values[i]);
            const bool bSelected = Value == Values[i];
            if (ImGui::Selectable(ButtonLabel, bSelected, 0, ImVec2(88.0f, 22.0f)))
            {
                Value = Values[i];
            }
            if ((i + 1) % 3 != 0 && i + 1 < Count)
            {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        ImGui::Unindent(12.0f);
        ImGui::EndDisabled();
    }

    void DrawShowFlagSliderFloat(const char *Label, float &Value, float Min, float Max, const char *Format = "%.2f")
    {
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderFloat(Label, &Value, Min, Max, Format);
    }

    void DrawShowFlagSliderInt(const char *Label, int32 &Value, int32 Min, int32 Max)
    {
        ImGui::SetNextItemWidth(150.0f);
        ImGui::SliderInt(Label, &Value, Min, Max);
    }
}

void FSkeletalMeshEditorToolbar::RenderViewportToolbar(USkeletalMesh *Mesh, FSkeletalMeshEditorState &State, FRenderer *Renderer)
{
    FEditorViewportToolbar::FDesc Desc;
    Desc.IdPrefix = "SkeletalMeshViewportToolbar";
    Desc.ToolbarWidth = ImGui::GetWindowWidth();
    Desc.Renderer = Renderer;
    Desc.bEnabled = Mesh != nullptr;
    Desc.ToolMode = ToCommonToolMode(State.GizmoMode);
    Desc.CoordSpace = State.GizmoSpace == EGizmoSpace::World ? FEditorViewportToolbar::ECoordSpace::World
                                                             : FEditorViewportToolbar::ECoordSpace::Local;
    Desc.bSnapEnabled = State.bEnableTranslationSnap || State.bEnableRotationSnap || State.bEnableScaleSnap;
    Desc.ViewportTypeIcon = GetViewportTypeIcon(State.PreviewViewportType);
    Desc.ViewportTypeLabel = GetViewportTypeLabel(State.PreviewViewportType);
    Desc.ViewModeIcon = GetViewModeIcon(State.PreviewViewMode);
    Desc.ViewModeLabel = GetViewModeLabel(State.PreviewViewMode);

    Desc.OnToolModeChanged = [&](FEditorViewportToolbar::EToolMode Mode)
    {
        State.bEnablePoseEditMode = true;
        State.GizmoMode = ToGizmoMode(Mode);
    };
    Desc.OnCoordSpaceToggled = [&]()
    {
        State.GizmoSpace = State.GizmoSpace == EGizmoSpace::World ? EGizmoSpace::Local : EGizmoSpace::World;
    };

    Desc.DrawSnapPopup = [&]()
    {
        static const float TranslationSnapSizes[] = {1.0f, 5.0f, 10.0f, 50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f};
        static const float RotationSnapSizes[] = {1.0f, 5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 60.0f, 90.0f};
        static const float ScaleSnapSizes[] = {0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};

        FEditorViewportToolbar::DrawPopupSectionHeader("SNAP SETTINGS");
        DrawSnapValueRow("Location", State.bEnableTranslationSnap, State.TranslationSnapSize, TranslationSnapSizes, 9, "%.0f");
        ImGui::Spacing();
        DrawSnapValueRow("Rotation", State.bEnableRotationSnap, State.RotationSnapSize, RotationSnapSizes, 8, "%.0f deg");
        ImGui::Spacing();
        DrawSnapValueRow("Scale", State.bEnableScaleSnap, State.ScaleSnapSize, ScaleSnapSizes, 8, "%.5g");
    };

    Desc.DrawCameraPopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("CAMERA");
        ImGui::TextDisabled("Preview camera settings");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(150.0f);
        ImGui::DragFloat("Speed", &State.CameraSpeed, 0.1f, 0.1f, 1000.0f, "%.1f");
        ImGui::SetNextItemWidth(150.0f);
        ImGui::DragFloat("FOV", &State.CameraFOV, 0.5f, 1.0f, 170.0f, "%.1f");
        ImGui::SetNextItemWidth(150.0f);
        ImGui::DragFloat("Ortho Width", &State.CameraOrthoWidth, 0.1f, 0.1f, 100000.0f, "%.1f");
        FEditorViewportToolbar::DrawPopupSeparator(4.0f, 6.0f);
        if (ImGui::MenuItem("Frame Selected"))
        {
            State.bFramePreviewRequested = true;
        }
        if (ImGui::MenuItem("Perspective", nullptr, State.PreviewViewportType == ESkeletalMeshPreviewViewportType::Perspective))
        {
            State.PreviewViewportType = ESkeletalMeshPreviewViewportType::Perspective;
        }
    };

    Desc.DrawViewportTypePopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("PERSPECTIVE");
        DrawViewportTypeOption(State, "Perspective", ESkeletalMeshPreviewViewportType::Perspective);
        FEditorViewportToolbar::DrawPopupSectionHeader("ORTHOGRAPHIC");
        DrawViewportTypeOption(State, "Top", ESkeletalMeshPreviewViewportType::Top);
        DrawViewportTypeOption(State, "Bottom", ESkeletalMeshPreviewViewportType::Bottom);
        DrawViewportTypeOption(State, "Left", ESkeletalMeshPreviewViewportType::Left);
        DrawViewportTypeOption(State, "Right", ESkeletalMeshPreviewViewportType::Right);
        DrawViewportTypeOption(State, "Front", ESkeletalMeshPreviewViewportType::Front);
        DrawViewportTypeOption(State, "Back", ESkeletalMeshPreviewViewportType::Back);
        DrawViewportTypeOption(State, "Free Ortho", ESkeletalMeshPreviewViewportType::FreeOrtho);
    };

    Desc.DrawViewModePopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("VIEW MODE");
        DrawViewModeOption(State, "Lit", ESkeletalMeshPreviewViewMode::Lit);
        DrawViewModeOption(State, "Unlit", ESkeletalMeshPreviewViewMode::Unlit);
        DrawViewModeOption(State, "Wireframe", ESkeletalMeshPreviewViewMode::Wireframe);
        DrawViewModeOption(State, "Lit Gouraud", ESkeletalMeshPreviewViewMode::LitGouraud);
        DrawViewModeOption(State, "Lit Lambert", ESkeletalMeshPreviewViewMode::LitLambert);
        DrawViewModeOption(State, "Scene Depth", ESkeletalMeshPreviewViewMode::SceneDepth);
        DrawViewModeOption(State, "World Normal", ESkeletalMeshPreviewViewMode::WorldNormal);
        DrawViewModeOption(State, "Light Culling", ESkeletalMeshPreviewViewMode::LightCulling);
    };

    Desc.DrawShowPopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("COMMON SHOW FLAGS");
        ImGui::Checkbox("Primitives", &State.bShowPrimitives);
        ImGui::Checkbox("SkeletalMesh", &State.bShowSkeletalMesh);
        ImGui::Checkbox("Billboard Text", &State.bShowBillboardText);

        FEditorViewportToolbar::DrawPopupSectionHeader("ACTOR HELPERS");
        ImGui::Checkbox("Grid", &State.bShowGrid);
        if (State.bShowGrid)
        {
            DrawShowFlagSliderFloat("Spacing", State.GridSpacing, 0.1f, 10.0f, "%.1f");
            DrawShowFlagSliderInt("Half Line Count", State.GridHalfLineCount, 10, 500);
            DrawShowFlagSliderFloat("Grid Line", State.GridLineThickness, 0.0f, 4.0f);
            DrawShowFlagSliderFloat("Major Line", State.GridMajorLineThickness, 0.0f, 6.0f);
            DrawShowFlagSliderInt("Major Interval", State.GridMajorLineInterval, 1, 50);
            DrawShowFlagSliderFloat("Minor Intensity", State.GridMinorIntensity, 0.0f, 2.0f);
            DrawShowFlagSliderFloat("Major Intensity", State.GridMajorIntensity, 0.0f, 2.0f);
        }
        ImGui::Checkbox("World Axis", &State.bShowWorldAxis);
        if (State.bShowWorldAxis)
        {
            DrawShowFlagSliderFloat("Axis Thickness", State.AxisThickness, 0.0f, 8.0f);
            DrawShowFlagSliderFloat("Axis Intensity", State.AxisIntensity, 0.0f, 2.0f);
        }
        ImGui::Checkbox("Gizmo", &State.bShowGizmo);
        ImGui::Checkbox("Bones", &State.bShowBones);
        DrawShowFlagSliderFloat("Billboard Icon Scale", State.BillboardIconScale, 0.1f, 5.0f);

        FEditorViewportToolbar::DrawPopupSectionHeader("SKELETAL PREVIEW");
        ImGui::Checkbox("Reference Pose", &State.bShowReferencePose);
        ImGui::Checkbox("Mesh Stats", &State.bShowMeshStatsOverlay);

        FEditorViewportToolbar::DrawPopupSectionHeader("DEBUG");
        DrawShowFlagSliderFloat("Line Thickness", State.DebugLineThickness, 1.0f, 12.0f, "%.1f");
        ImGui::Checkbox("Scene BVH (Green)", &State.bShowSceneBVH);
        ImGui::Checkbox("Scene Octree (Cyan)", &State.bShowOctree);
        ImGui::Checkbox("World Bound (Magenta)", &State.bShowWorldBound);
        ImGui::Checkbox("Light Visualization", &State.bShowLightVisualization);
    };

    Desc.DrawLayoutPopup = [&]()
    {
        FEditorViewportToolbar::DrawPopupSectionHeader("VIEWPORT LAYOUT");
        auto DrawLayoutItem = [&](const char *Label, ESkeletalMeshPreviewLayout Layout)
        {
            if (ImGui::MenuItem(Label, nullptr, State.PreviewLayout == Layout))
            {
                State.PreviewLayout = Layout;
                ImGui::CloseCurrentPopup();
            }
        };
        DrawLayoutItem("One Pane", ESkeletalMeshPreviewLayout::OnePane);
        DrawLayoutItem("Two Panes - Horizontal", ESkeletalMeshPreviewLayout::TwoPanesHorizontal);
        DrawLayoutItem("Two Panes - Vertical", ESkeletalMeshPreviewLayout::TwoPanesVertical);
        DrawLayoutItem("Four Panes 2 x 2", ESkeletalMeshPreviewLayout::FourPanes2x2);
        FEditorViewportToolbar::DrawPopupSeparator(4.0f, 6.0f);
        ImGui::TextDisabled("Skeletal Mesh Editor viewport still renders a single pane until split layout is wired.");
    };

    FEditorViewportToolbar::RenderViewportToolbar(Desc);
}
