#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Object/Object.h"

class UActorComponent;
class UClass;
class AActor;

class FEditorDetailsWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;
	void SetShowEditorOnlyComponents(bool bEnable) { bShowEditorOnlyComponents = bEnable; }
	bool IsShowingEditorOnlyComponents() const { return bShowEditorOnlyComponents; }

private:
	void RenderHeader(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderAddComponentButton(AActor* Actor);
	void RenderComponentTree(AActor* Actor);
	void RenderSceneComponentNode(class USceneComponent* Comp);
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties(AActor* Actor, const TArray<AActor*>& SelectedActors);
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	bool RenderPropertyWidget(TArray<struct FPropertyDescriptor>& Props, int32& Index);
	void CommitActorNameEdit(AActor* Actor);

	void PropagatePropertyChange(const FString& PropName, const TArray<AActor*>& SelectedActors);

	static FString OpenObjFileDialog();

	UActorComponent* SelectedComponent = nullptr;
	AActor* LastSelectedActor = nullptr;
	bool bActorSelected = true;
	bool bShowEditorOnlyComponents = false;
	bool bEditingActorName = false;
	char ActorNameBuffer[256] = {};
	UClass* SelectedAddComponentClass = nullptr;
};
