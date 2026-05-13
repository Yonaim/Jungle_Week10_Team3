#pragma once

#include "Core/CoreTypes.h"

// 에디터 우하단 토스트 알림 렌더러
// FNotificationManager(Engine)의 큐를 읽어 ImGui로 표시한다.
namespace FNotificationToast
{
    void Render();

    // 동기 import처럼 작업 중간 frame이 적은 작업도 완료 후 진행 로그/진행률을 볼 수 있게 하는 공통 progress toast.
    void BeginProgress(const FString &Title, const FString &Message, float Progress01 = 0.0f);
    void UpdateProgress(const FString &Message, float Progress01);
    void FinishProgress(const FString &Message, bool bSuccess, float HoldSeconds = 4.0f);
}
