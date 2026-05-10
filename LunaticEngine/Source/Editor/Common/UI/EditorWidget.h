#pragma once

#include "Core/CoreTypes.h"

class UEditorEngine;

class FEditorUIElement
{
public:
	virtual ~FEditorUIElement() = default;

	virtual void Init(UEditorEngine* InEditorEngine);
	virtual void Render(float DeltaTime) = 0;

protected:
	UEditorEngine* EditorEngine = nullptr;
};
