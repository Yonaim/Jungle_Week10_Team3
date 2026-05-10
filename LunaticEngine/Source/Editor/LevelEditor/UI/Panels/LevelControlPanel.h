#pragma once

#include "Editor/Common/UI/EditorWidget.h"

class FLevelControlPanel : public FEditorUIElement
{
  public:
    virtual void Render(float DeltaTime) override;
};
