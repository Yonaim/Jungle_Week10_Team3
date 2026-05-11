#pragma once

#include "Core/CoreTypes.h"

class IEditorMenuProvider
{
  public:
    virtual ~IEditorMenuProvider() = default;

    virtual void BuildFileMenu() {}
    virtual void BuildEditMenu() {}
    virtual void BuildWindowMenu() {}
    virtual void BuildCustomMenus() {}

    virtual FString GetFrameTitle() const;
    virtual FString GetFrameTitleTooltip() const;
};
