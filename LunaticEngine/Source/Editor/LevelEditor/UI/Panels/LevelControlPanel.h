#pragma once

#include "Common/UI/EditorUIElement.h"

class FLevelControlPanel : public FEditorUIElement
{
  public:
    virtual void Render(float DeltaTime) override;
};
