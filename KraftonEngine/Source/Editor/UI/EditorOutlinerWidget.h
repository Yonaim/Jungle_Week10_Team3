#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/CoreTypes.h"

class FEditorOutlinerWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	void RenderActorOutliner();
	bool DrawVisibilityToggle(const char* Id, bool bVisible) const;

	TArray<int32> ValidActorIndices;
	char SearchBuffer[128] = {};
};
