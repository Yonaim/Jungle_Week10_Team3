#include "Editor/UI/EditorSceneWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/SelectionManager.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include "ImGui/imgui.h"
#include "Profiling/Stats.h"

#include <cstring>

void FEditorOutlinerWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorOutlinerWidget::Render(float DeltaTime)
{
	if (!EditorEngine)
	{
		return;
	}

	(void)DeltaTime;
	ImGui::SetNextWindowSize(ImVec2(520.0f, 420.0f), ImGuiCond_Once);

	if (!ImGui::Begin("Outliner"))
	{
		ImGui::End();
		return;
	}

	ImGui::InputTextWithHint("##OutlinerSearch", "Search...", SearchBuffer, sizeof(SearchBuffer));
	ImGui::Separator();
	RenderActorOutliner();
	ImGui::End();
}

bool FEditorOutlinerWidget::DrawVisibilityToggle(const char* Id, bool bVisible) const
{
	const ImVec2 Size(20.0f, 16.0f);
	if (!ImGui::InvisibleButton(Id, Size))
	{
	}

	const bool bClicked = ImGui::IsItemClicked();
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	const ImU32 Stroke = ImGui::GetColorU32(bVisible ? ImVec4(0.88f, 0.88f, 0.88f, 1.0f) : ImVec4(0.42f, 0.42f, 0.42f, 1.0f));
	const ImU32 Fill = ImGui::GetColorU32(bVisible ? ImVec4(0.88f, 0.88f, 0.88f, 0.15f) : ImVec4(0.30f, 0.30f, 0.30f, 0.10f));

	DrawList->AddEllipseFilled(Center, ImVec2(6.0f, 4.0f), Fill);
	DrawList->AddEllipse(Center, ImVec2(6.0f, 4.0f), Stroke, 0.0f, 24, 1.5f);
	DrawList->AddCircleFilled(Center, 1.8f, Stroke, 12);

	if (!bVisible)
	{
		DrawList->AddLine(ImVec2(Min.x + 4.0f, Max.y - 3.0f), ImVec2(Max.x - 4.0f, Min.y + 3.0f), Stroke, 1.5f);
	}

	return bClicked;
}

void FEditorOutlinerWidget::RenderActorOutliner()
{
	SCOPE_STAT_CAT("OutlinerWidget::ActorOutliner", "5_UI");

	UWorld* World = EditorEngine->GetWorld();
	if (!World)
	{
		return;
	}

	const TArray<AActor*> Actors = World->GetActors().ToArray();
	FSelectionManager& Selection = EditorEngine->GetSelectionManager();

	ValidActorIndices.clear();
	ValidActorIndices.reserve(Actors.size());

	for (int32 i = 0; i < static_cast<int32>(Actors.size()); ++i)
	{
		AActor* Actor = Actors[i];
		if (!Actor)
		{
			continue;
		}

		const FString Label = Actor->GetFName().ToString();
		const FString Type = Actor->GetClass()->GetName();
		if (SearchBuffer[0] != '\0')
		{
			const bool bMatchesLabel = strstr(Label.c_str(), SearchBuffer) != nullptr;
			const bool bMatchesType = strstr(Type.c_str(), SearchBuffer) != nullptr;
			if (!bMatchesLabel && !bMatchesType)
			{
				continue;
			}
		}

		ValidActorIndices.push_back(i);
	}

	if (!ImGui::BeginTable("##OutlinerTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
	{
		return;
	}

	ImGui::TableSetupColumn(" ", ImGuiTableColumnFlags_WidthFixed, 28.0f);
	ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch, 260.0f);
	ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 140.0f);
	ImGui::TableHeadersRow();

	ImGuiListClipper Clipper;
	Clipper.Begin(static_cast<int>(ValidActorIndices.size()));
	while (Clipper.Step())
	{
		for (int Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row)
		{
			AActor* Actor = Actors[ValidActorIndices[Row]];
			if (!Actor)
			{
				continue;
			}

			const FString Label = Actor->GetFName().ToString().empty() ? Actor->GetClass()->GetName() : Actor->GetFName().ToString();
			const FString Type = Actor->GetClass()->GetName();
			const bool bIsSelected = Selection.IsSelected(Actor);

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			const FString VisibilityId = "##Vis" + std::to_string(ValidActorIndices[Row]);
			if (DrawVisibilityToggle(VisibilityId.c_str(), Actor->IsVisible()))
			{
				Actor->SetVisible(!Actor->IsVisible());
			}

			ImGui::TableSetColumnIndex(1);
			if (ImGui::Selectable((Label + "##OutlinerRow" + std::to_string(ValidActorIndices[Row])).c_str(), bIsSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				if (ImGui::GetIO().KeyShift)
				{
					Selection.SelectRange(Actor, Actors);
				}
				else if (ImGui::GetIO().KeyCtrl)
				{
					Selection.ToggleSelect(Actor);
				}
				else
				{
					Selection.Select(Actor);
				}
			}

			ImGui::TableSetColumnIndex(2);
			ImGui::TextDisabled("%s", Type.c_str());
		}
	}

	ImGui::EndTable();
}
