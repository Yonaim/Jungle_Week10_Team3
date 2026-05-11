#pragma once

#include "Common/Gizmo/GizmoTypes.h"
#include "Common/Gizmo/TransformProxy.h"
#include "ImGui/imgui.h"
#include "Math/Vector.h"

#include <cmath>
#include <memory>

namespace GizmoManagerPrivate
{
    inline float DistanceToSegmentSq(const ImVec2& P, const ImVec2& A, const ImVec2& B)
    {
        const float ABx = B.x - A.x;
        const float ABy = B.y - A.y;
        const float APx = P.x - A.x;
        const float APy = P.y - A.y;
        const float LenSq = ABx * ABx + ABy * ABy;
        const float T = LenSq > 0.0001f ? (APx * ABx + APy * ABy) / LenSq : 0.0f;
        const float ClampedT = T < 0.0f ? 0.0f : (T > 1.0f ? 1.0f : T);
        const float Cx = A.x + ABx * ClampedT;
        const float Cy = A.y + ABy * ClampedT;
        const float Dx = P.x - Cx;
        const float Dy = P.y - Cy;
        return Dx * Dx + Dy * Dy;
    }

    inline bool IsPointInsideRect(const ImVec2& P, float X, float Y, float W, float H)
    {
        return P.x >= X && P.x <= X + W && P.y >= Y && P.y <= Y + H;
    }
}

// ImGui overlay 기반의 최소 공용 기즈모 매니저.
// - LevelEditor에는 아직 연결하지 않는다.
// - 현재 SkeletalMeshEditor에서 BoneTransformProxy를 대상으로 사용한다.
// - 렌더러 primitive gizmo가 준비되기 전까지 viewport overlay로 표시/입력 처리한다.
class FGizmoManager
{
  public:
    void SetTarget(std::shared_ptr<ITransformProxy> InTarget);
    void ClearTarget();

    bool HasValidTarget() const;

    void       SetMode(EGizmoMode InMode) { Mode = InMode; }
    EGizmoMode GetMode() const { return Mode; }

    void        SetSpace(EGizmoSpace InSpace) { Space = InSpace; }
    EGizmoSpace GetSpace() const { return Space; }

    bool IsDragging() const { return bDragging; }

    // ViewportClient가 ProjectWorldToViewport를 넘겨주면 여기서 화면 기즈모를 그리고 입력을 처리한다.
    // 반환값: 기즈모가 마우스 입력을 소비했으면 true.
    template <typename ProjectFunc>
    bool DrawAndHandle(float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight, ProjectFunc &&ProjectWorldToScreen,
                       const FVector &CameraRight, const FVector &CameraUp, float ViewDistance)
    {
        return DrawAndHandleInternal(ViewportX, ViewportY, ViewportWidth, ViewportHeight, ProjectWorldToScreen, CameraRight, CameraUp,
                                     ViewDistance);
    }

  private:
    template <typename ProjectFunc>
    bool DrawAndHandleInternal(float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight,
                               ProjectFunc &&ProjectWorldToScreen, const FVector &CameraRight, const FVector &CameraUp, float ViewDistance);

  private:
    std::shared_ptr<ITransformProxy> Target;

    EGizmoMode  Mode = EGizmoMode::Translate;
    EGizmoSpace Space = EGizmoSpace::Local;

    bool       bDragging = false;
    bool       bHot = false;
    FTransform DragStartLocalTransform;
    FTransform DragStartWorldTransform;
};

// ================================ IMPLEMENT ==================================
template <typename ProjectFunc>
bool FGizmoManager::DrawAndHandleInternal(float ViewportX, float ViewportY, float ViewportWidth, float ViewportHeight,
                                          ProjectFunc&& ProjectWorldToScreen, const FVector& CameraRight, const FVector& CameraUp,
                                          float ViewDistance)
{
    if (!HasValidTarget() || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
    {
        bDragging = false;
        bHot = false;
        return false;
    }

    const FTransform WorldTransform = Target->GetWorldTransform();
    const FVector WorldLocation = WorldTransform.GetLocation();

    ImVec2 Center;
    if (!ProjectWorldToScreen(WorldLocation, Center))
    {
        return false;
    }

    ImDrawList* DrawList = ImGui::GetForegroundDrawList();
    const float AxisLength = 52.0f;
    const ImVec2 XEnd(Center.x + AxisLength, Center.y);
    const ImVec2 YEnd(Center.x, Center.y - AxisLength);
    const ImVec2 ZEnd(Center.x - AxisLength * 0.55f, Center.y + AxisLength * 0.55f);

    const ImVec2 Mouse = ImGui::GetIO().MousePos;
    const bool bMouseInsideViewport = GizmoManagerPrivate::IsPointInsideRect(Mouse, ViewportX, ViewportY, ViewportWidth, ViewportHeight);
    const float HitDistanceSq = 10.0f * 10.0f;
    bHot = bMouseInsideViewport && (GizmoManagerPrivate::DistanceToSegmentSq(Mouse, Center, XEnd) <= HitDistanceSq ||
                                    GizmoManagerPrivate::DistanceToSegmentSq(Mouse, Center, YEnd) <= HitDistanceSq ||
                                    GizmoManagerPrivate::DistanceToSegmentSq(Mouse, Center, ZEnd) <= HitDistanceSq ||
                                    GizmoManagerPrivate::DistanceToSegmentSq(Mouse, ImVec2(Center.x - 8.0f, Center.y),
                                                                             ImVec2(Center.x + 8.0f, Center.y)) <= HitDistanceSq);

    const float Thickness = bHot || bDragging ? 3.0f : 2.0f;
    DrawList->AddCircleFilled(Center, bHot || bDragging ? 5.5f : 4.0f, IM_COL32(255, 230, 80, 255));
    DrawList->AddLine(Center, XEnd, IM_COL32(235, 80, 80, 255), Thickness);
    DrawList->AddLine(Center, YEnd, IM_COL32(80, 235, 80, 255), Thickness);
    DrawList->AddLine(Center, ZEnd, IM_COL32(80, 140, 255, 255), Thickness);
    DrawList->AddText(ImVec2(XEnd.x + 4.0f, XEnd.y - 7.0f), IM_COL32(235, 80, 80, 255), "X");
    DrawList->AddText(ImVec2(YEnd.x - 4.0f, YEnd.y - 16.0f), IM_COL32(80, 235, 80, 255), "Y");
    DrawList->AddText(ImVec2(ZEnd.x - 14.0f, ZEnd.y + 2.0f), IM_COL32(80, 140, 255, 255), "Z");

    if (!bDragging && bHot && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        bDragging = true;
        DragStartLocalTransform = Target->GetLocalTransform();
        DragStartWorldTransform = Target->GetWorldTransform();
        Target->BeginTransform();
    }

    if (bDragging)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (Mode == EGizmoMode::Translate)
            {
                const ImVec2 Delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
                const float PixelToWorld = (ViewDistance > 0.01f ? ViewDistance : 1.0f) * 0.0015f;

                FTransform NewLocal = DragStartLocalTransform;
                FVector NewLocation = DragStartLocalTransform.GetLocation();
                NewLocation += CameraRight * (Delta.x * PixelToWorld);
                NewLocation -= CameraUp * (Delta.y * PixelToWorld);
                NewLocal.SetLocation(NewLocation);
                Target->SetLocalTransform(NewLocal);
            }
        }
        else
        {
            Target->EndTransform();
            bDragging = false;
        }

        return true;
    }

    return bHot;
}
