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
	void RenderComponentTree(AActor* Actor, float Height);
	void RenderSceneComponentNode(class USceneComponent* Comp);
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties(AActor* Actor, const TArray<AActor*>& SelectedActors);
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	bool RenderPropertyWidget(TArray<struct FPropertyDescriptor>& Props, int32& Index);
	void CommitActorNameEdit(AActor* Actor);
	void RenderPropertySection(const char* SectionName, TArray<struct FPropertyDescriptor>& Props, const TArray<int32>& Indices, const TArray<AActor*>& SelectedActors, bool& bAnyChanged);
	void RenderDetailsFilterBar(const TArray<const char*>& AvailableSections);
	bool ShouldDisplaySection(const char* SectionName, const TArray<struct FPropertyDescriptor>& Props, const TArray<int32>& Indices) const;
	bool SectionMatchesSearch(const char* SectionName, const TArray<struct FPropertyDescriptor>& Props, const TArray<int32>& Indices) const;

	void PropagatePropertyChange(const FString& PropName, const TArray<AActor*>& SelectedActors);

	static FString OpenObjFileDialog();
	static FString GetDisplayPropertyLabel(const FString& RawName);
	static bool DrawColoredFloat3(const char* Label, float Values[3], float Speed);
	static bool DrawColoredFloat4(const char* Label, float Values[4], float Speed);
	static FString GetPropertySectionName(const struct FPropertyDescriptor& Prop);

	UActorComponent* SelectedComponent = nullptr;
	AActor* LastSelectedActor = nullptr;
	AActor* LockedActor = nullptr;
	bool bActorSelected = true;
	bool bShowEditorOnlyComponents = false;
	bool bEditingActorName = false;
	bool bSelectionLocked = false;
	char ActorNameBuffer[256] = {};
	char DetailSearchBuffer[128] = {};
	FString ActiveSectionFilter = "All";
	UClass* SelectedAddComponentClass = nullptr;
	float ComponentTreeHeight = 160.0f;
};
