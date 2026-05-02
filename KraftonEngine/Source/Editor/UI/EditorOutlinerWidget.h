#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/CoreTypes.h"

class AActor;

class FEditorOutlinerWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	struct FOutlinerDragPayload
	{
		AActor* Actor = nullptr;
	};

	void SelectAllVisibleActors();
	void RenderActorOutliner();
	bool DrawVisibilityToggle(const char* Id, bool bVisible) const;
	void HandleActorDrop(AActor* DraggedActor, AActor* TargetActor) const;
	void HandleFolderDrop(AActor* DraggedActor, const FString& FolderName) const;
	void HandleRootDrop(AActor* DraggedActor) const;

	TArray<int32> ValidActorIndices;
	char SearchBuffer[128] = {};
	char NewFolderBuffer[128] = {};
	FString TypeFilter = "All Types";
};
