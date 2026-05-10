#pragma once

#include "Common/Viewport/EditorViewportClient.h"

// UE??FLevelEditorViewportClient ???
// ?덈꺼 ?몄쭛 ?꾩슜 酉고룷??(移대찓??議곗옉, 湲곗쫰紐? ?≫꽣 ?쇳궧 ??
// ?꾩옱??FEditorViewportClient??湲곗〈 湲곕뒫??洹몃?濡??ъ슜
class FLevelEditorViewportClient : public FEditorViewportClient
{
  public:
    FLevelEditorViewportClient() = default;
    ~FLevelEditorViewportClient() override = default;
};
