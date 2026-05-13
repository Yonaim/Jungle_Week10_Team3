#include "PCH/LunaticPCH.h"
#include "Common/UI/Notifications/NotificationToast.h"
#include "Core/Notification.h"
#include "ImGui/imgui.h"
#include <algorithm>
#include <vector>

namespace FNotificationToast
{
namespace
{
    struct FProgressToastState
    {
        bool bVisible = false;
        bool bFinished = false;
        bool bSuccess = true;
        float Progress01 = 0.0f;
        float HoldSeconds = 0.0f;
        float ElapsedAfterFinish = 0.0f;
        FString Title;
        FString Message;
        std::vector<FString> Steps;
    };

    FProgressToastState &GetProgressState()
    {
        static FProgressToastState State;
        return State;
    }

    void PushProgressStep(FProgressToastState &State, const FString &Message)
    {
        if (!Message.empty())
        {
            State.Steps.push_back(Message);
            if (State.Steps.size() > 5)
            {
                State.Steps.erase(State.Steps.begin());
            }
        }
    }
}

void BeginProgress(const FString &Title, const FString &Message, float Progress01)
{
    FProgressToastState &State = GetProgressState();
    State = FProgressToastState{};
    State.bVisible = true;
    State.bFinished = false;
    State.bSuccess = true;
    State.Progress01 = std::clamp(Progress01, 0.0f, 1.0f);
    State.Title = Title;
    State.Message = Message;
    PushProgressStep(State, Message);
}

void UpdateProgress(const FString &Message, float Progress01)
{
    FProgressToastState &State = GetProgressState();
    if (!State.bVisible)
    {
        BeginProgress("Working", Message, Progress01);
        return;
    }

    State.Message = Message;
    State.Progress01 = std::clamp(Progress01, 0.0f, 1.0f);
    PushProgressStep(State, Message);
}

void FinishProgress(const FString &Message, bool bSuccess, float HoldSeconds)
{
    FProgressToastState &State = GetProgressState();
    if (!State.bVisible)
    {
        BeginProgress(bSuccess ? "Completed" : "Failed", Message, bSuccess ? 1.0f : State.Progress01);
    }

    State.Message = Message;
    State.bFinished = true;
    State.bSuccess = bSuccess;
    State.Progress01 = bSuccess ? 1.0f : State.Progress01;
    State.HoldSeconds = HoldSeconds;
    State.ElapsedAfterFinish = 0.0f;
    PushProgressStep(State, Message);
}

void Render()
{
    const auto& Notifications = FNotificationManager::Get().GetNotifications();
    const ImGuiViewport* VP = ImGui::GetMainViewport();
    if (!VP)
    {
        return;
    }

    ImDrawList* DrawList = ImGui::GetForegroundDrawList();

    const float Padding = 16.0f;
    const float ToastMaxWidth = 400.0f;
    const float ToastPadX = 12.0f;
    const float ToastPadY = 10.0f;
    const float Spacing = 6.0f;
    const float FadeTime = 0.4f;
    const float Rounding = 6.0f;

    // 아래에서 위로 쌓임
    float OffsetY = VP->Pos.y + VP->Size.y - Padding;

    FProgressToastState &Progress = GetProgressState();
    if (Progress.bVisible)
    {
        if (Progress.bFinished)
        {
            Progress.ElapsedAfterFinish += ImGui::GetIO().DeltaTime;
            if (Progress.ElapsedAfterFinish >= Progress.HoldSeconds)
            {
                Progress.bVisible = false;
            }
        }

        if (Progress.bVisible)
        {
            const float ProgressW = 420.0f;
            const float LineH = ImGui::GetTextLineHeight();
            const int32 StepCount = static_cast<int32>(Progress.Steps.size());
            const float ProgressH = 70.0f + (std::min)(StepCount, 4) * (LineH + 2.0f);
            OffsetY -= ProgressH;
            const float PosX = VP->Pos.x + VP->Size.x - ProgressW - Padding;
            const float PosY = OffsetY;
            const ImVec4 Bg = Progress.bFinished
                ? (Progress.bSuccess ? ImVec4(0.12f, 0.46f, 0.24f, 0.95f) : ImVec4(0.64f, 0.14f, 0.14f, 0.95f))
                : ImVec4(0.18f, 0.18f, 0.23f, 0.95f);
            const ImU32 BgColor = ImGui::ColorConvertFloat4ToU32(Bg);
            const ImU32 TextColor = IM_COL32(245, 245, 248, 255);
            const ImU32 MutedColor = IM_COL32(185, 185, 192, 255);
            const ImU32 BarBgColor = IM_COL32(20, 20, 22, 255);
            const ImU32 BarFillColor = IM_COL32(70, 150, 255, 255);

            DrawList->AddRectFilled(ImVec2(PosX, PosY), ImVec2(PosX + ProgressW, PosY + ProgressH), BgColor, Rounding);
            DrawList->AddText(ImVec2(PosX + ToastPadX, PosY + ToastPadY), TextColor,
                              Progress.Title.empty() ? "Progress" : Progress.Title.c_str());
            DrawList->AddText(ImVec2(PosX + ToastPadX, PosY + ToastPadY + LineH + 2.0f), MutedColor,
                              Progress.Message.empty() ? "Working..." : Progress.Message.c_str());

            const float BarX = PosX + ToastPadX;
            const float BarY = PosY + ToastPadY + LineH * 2.0f + 10.0f;
            const float BarW = ProgressW - ToastPadX * 2.0f;
            const float BarH = 7.0f;
            DrawList->AddRectFilled(ImVec2(BarX, BarY), ImVec2(BarX + BarW, BarY + BarH), BarBgColor, 3.5f);
            DrawList->AddRectFilled(ImVec2(BarX, BarY), ImVec2(BarX + BarW * Progress.Progress01, BarY + BarH), BarFillColor, 3.5f);

            float StepY = BarY + BarH + 8.0f;
            const int32 FirstStep = (std::max)(0, StepCount - 4);
            for (int32 Index = FirstStep; Index < StepCount; ++Index)
            {
                const FString StepLine = std::string("- ") + Progress.Steps[Index];
                DrawList->AddText(ImVec2(PosX + ToastPadX, StepY), MutedColor, StepLine.c_str());
                StepY += LineH + 2.0f;
            }
            OffsetY -= Spacing;
        }
    }

    for (int i = (int)Notifications.size() - 1; i >= 0; --i)
    {
        const FNotification& N = Notifications[i];

        float Alpha = 1.0f;
        if (N.ElapsedTime < FadeTime)
        {
            Alpha = N.ElapsedTime / FadeTime;
        }
        else if (N.Duration - N.ElapsedTime < FadeTime)
        {
            Alpha = (N.Duration - N.ElapsedTime) / FadeTime;
        }
        Alpha = std::clamp(Alpha, 0.0f, 1.0f);

        ImVec2 TextSize = ImGui::CalcTextSize(N.Message.c_str(), nullptr, false, ToastMaxWidth - ToastPadX * 2.0f);
        float ToastW = TextSize.x + ToastPadX * 2.0f;
        float ToastH = TextSize.y + ToastPadY * 2.0f;
        if (ToastW > ToastMaxWidth) ToastW = ToastMaxWidth;

        OffsetY -= ToastH;
        float PosX = VP->Pos.x + VP->Size.x - ToastW - Padding;
        float PosY = OffsetY;

        ImVec4 BgColor;
        switch (N.Type)
        {
        case ENotificationType::Success:
            BgColor = ImVec4(0.12f, 0.55f, 0.28f, Alpha * 0.92f);
            break;
        case ENotificationType::Error:
            BgColor = ImVec4(0.72f, 0.15f, 0.15f, Alpha * 0.92f);
            break;
        default:
            BgColor = ImVec4(0.22f, 0.22f, 0.28f, Alpha * 0.92f);
            break;
        }

        DrawList->AddRectFilled(ImVec2(PosX, PosY), ImVec2(PosX + ToastW, PosY + ToastH),
                                ImGui::ColorConvertFloat4ToU32(BgColor), Rounding);

        ImU32 TextCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, Alpha));
        DrawList->AddText(nullptr, 0.0f, ImVec2(PosX + ToastPadX, PosY + ToastPadY),
                          TextCol, N.Message.c_str(), nullptr, ToastMaxWidth - ToastPadX * 2.0f);

        OffsetY -= Spacing;
    }
}

} // namespace FNotificationToast
