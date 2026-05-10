#pragma once

#include "Editor/Common/UI/EditorWidget.h"

class FEditorControlWidget : public FEditorWidget
{
  public:
    virtual void Render(float DeltaTime) override;
};
