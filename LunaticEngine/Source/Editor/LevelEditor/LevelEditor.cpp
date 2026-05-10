#include "LevelEditor/LevelEditor.h"

// 현재 단계에서는 FLevelEditor가 Selection / ViewportLayout / OverlayStatSystem을
// 소유하는 얇은 컨테이너 역할만 한다.
// 초기화/해제 순서는 기존 UEditorEngine.cpp에서 유지하고,
// 이후 단계에서 Initialize / Shutdown으로 이동한다.
