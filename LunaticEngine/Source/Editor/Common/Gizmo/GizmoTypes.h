#pragma once

#include "Core/CoreTypes.h"

// Editor 공용 기즈모 동작 모드.
// 현재 SkeletalMeshEditor에서는 Translate만 실제 적용하고,
// Rotate / Scale은 툴바 상태와 향후 확장을 위해 먼저 열어둔다.
enum class EGizmoMode : uint8
{
    Translate = 0,
    Rotate,
    Scale,
};

// 기즈모 축 기준.
// World: 회전이 없는 월드 축 기준
// Local: 대상 Transform의 회전 기준
// 현재 2D ImGui overlay 구현은 Translate 중심이므로 Local/World 선택값만 보관한다.
enum class EGizmoSpace : uint8
{
    World = 0,
    Local,
};
