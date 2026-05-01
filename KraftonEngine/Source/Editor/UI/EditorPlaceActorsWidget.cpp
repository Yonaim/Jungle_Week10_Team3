#include "Editor/UI/EditorPlaceActorsWidget.h"

#include "Component/CameraComponent.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Resource/ResourceManager.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>

namespace
{
	using EPlaceType = FLevelViewportLayout::EViewportPlaceActorType;
	using ECategory = FEditorPlaceActorsWidget::EPlaceActorCategory;
	using FEntry = FEditorPlaceActorsWidget::FPlaceActorEntry;

	FString GetEditorPathResource(const char* Key)
	{
		return FResourceManager::Get().ResolvePath(FName(Key));
	}

	const FEntry GPlaceActorEntries[] = {
		{ "Actor", "actor empty basic", EPlaceType::Actor, ECategory::Basic },
		{ "Static Mesh Actor", "static mesh actor mesh basic", EPlaceType::StaticMeshActor, ECategory::Basic },
		{ "Ambient Light", "ambient light", EPlaceType::AmbientLight, ECategory::Lights },
		{ "Directional Light", "directional light sun", EPlaceType::DirectionalLight, ECategory::Lights },
		{ "Point Light", "point light bulb", EPlaceType::PointLight, ECategory::Lights },
		{ "Spot Light", "spot light cone", EPlaceType::SpotLight, ECategory::Lights },
		{ "Cube", "cube box shape", EPlaceType::Cube, ECategory::Shapes },
		{ "Sphere", "sphere ball shape", EPlaceType::Sphere, ECategory::Shapes },
		{ "Cylinder", "cylinder shape", EPlaceType::Cylinder, ECategory::Shapes },
		{ "Cone", "cone shape", EPlaceType::Cone, ECategory::Shapes },
		{ "Plane", "plane quad floor shape", EPlaceType::Plane, ECategory::Shapes },
		{ "Decal", "decal projection vfx", EPlaceType::Decal, ECategory::VFX },
		{ "Height Fog", "height fog vfx atmosphere", EPlaceType::HeightFog, ECategory::VFX },
	};

	const char* GetCategoryLabel(ECategory Category)
	{
		switch (Category)
		{
		case ECategory::Basic: return "Basic";
		case ECategory::Lights: return "Lights";
		case ECategory::Shapes: return "Shapes";
		case ECategory::VFX: return "VFX";
		default: return "Basic";
		}
	}
}

void FEditorPlaceActorsWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(420.0f, 640.0f), ImGuiCond_Once);
	if (!ImGui::Begin("Place Actors"))
	{
		ImGui::End();
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);

	if (ID3D11ShaderResourceView* AddActorIcon = FResourceManager::Get().FindLoadedTexture(
		GetEditorPathResource("Editor.ToolIcon.AddActor")).Get())
	{
		ImGui::Image(AddActorIcon, ImVec2(16.0f, 16.0f));
		ImGui::SameLine(0.0f, 8.0f);
	}
	ImGui::TextUnformatted("Place Actors");
	ImGui::Spacing();

	ImGui::InputTextWithHint("##PlaceActorSearch", "Search Classes", SearchBuffer, sizeof(SearchBuffer));
	ImGui::Spacing();

	if (ImGui::BeginTable("##PlaceActorsLayout", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Categories", ImGuiTableColumnFlags_WidthFixed, 116.0f);
		ImGui::TableSetupColumn("Actors", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextColumn();
		RenderCategorySidebar();

		ImGui::TableNextColumn();
		RenderActorGrid();

		ImGui::EndTable();
	}

	ImGui::PopStyleVar(2);
	ImGui::End();
}

void FEditorPlaceActorsWidget::RenderCategorySidebar()
{
	if (!ImGui::BeginChild("##PlaceActorCategories", ImVec2(0.0f, 0.0f), true))
	{
		ImGui::EndChild();
		return;
	}

	const EPlaceActorCategory Categories[] = {
		EPlaceActorCategory::Basic,
		EPlaceActorCategory::Lights,
		EPlaceActorCategory::Shapes,
		EPlaceActorCategory::VFX
	};

	for (EPlaceActorCategory Category : Categories)
	{
		const bool bSelected = (ActiveCategory == Category);
		if (bSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.42f, 0.80f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.48f, 0.88f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.36f, 0.72f, 1.0f));
		}

		if (ImGui::Button(GetCategoryLabel(Category), ImVec2(-1.0f, 42.0f)))
		{
			ActiveCategory = Category;
		}

		if (bSelected)
		{
			ImGui::PopStyleColor(3);
		}
	}

	ImGui::EndChild();
}

void FEditorPlaceActorsWidget::RenderActorGrid()
{
	if (!ImGui::BeginChild("##PlaceActorGrid", ImVec2(0.0f, 0.0f), false))
	{
		ImGui::EndChild();
		return;
	}

	ImGui::TextUnformatted(GetCategoryLabel(ActiveCategory));
	ImGui::Separator();

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float MinCardWidth = 128.0f;
	const int32 ColumnCount = (std::max)(1, static_cast<int32>(AvailableWidth / MinCardWidth));

	bool bAnyVisible = false;
	if (ImGui::BeginTable("##PlaceActorButtons", ColumnCount, ImGuiTableFlags_SizingStretchSame))
	{
		for (const FPlaceActorEntry& Entry : GPlaceActorEntries)
		{
			if (Entry.Category != ActiveCategory || !MatchesSearch(Entry))
			{
				continue;
			}

			bAnyVisible = true;
			ImGui::TableNextColumn();
			ImGui::PushID(Entry.Label);

			if (ImGui::Button(Entry.Label, ImVec2(-FLT_MIN, 54.0f)))
			{
				SpawnActor(Entry);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", Entry.Label);
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if (!bAnyVisible)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("No actors match the current search.");
	}

	ImGui::EndChild();
}

bool FEditorPlaceActorsWidget::MatchesSearch(const FPlaceActorEntry& Entry) const
{
	if (SearchBuffer[0] == '\0')
	{
		return true;
	}

	FString Query = SearchBuffer;
	std::transform(Query.begin(), Query.end(), Query.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });

	FString SearchKey = Entry.SearchKey;
	std::transform(SearchKey.begin(), SearchKey.end(), SearchKey.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });

	return SearchKey.find(Query) != FString::npos;
}

void FEditorPlaceActorsWidget::SpawnActor(const FPlaceActorEntry& Entry)
{
	if (!EditorEngine)
	{
		return;
	}

	UWorld* World = EditorEngine->GetWorld();
	UCameraComponent* Camera = EditorEngine->GetCamera();
	if (!World || !Camera)
	{
		return;
	}

	const FVector RayOrigin = Camera->GetWorldLocation();
	const FVector RayDirection = Camera->GetForwardVector().Normalized();
	FVector SpawnLocation = RayOrigin + RayDirection * 10.0f;

	FRayHitResult HitResult{};
	AActor* HitActor = nullptr;
	if (World->RaycastPrimitives(FRay{ RayOrigin, RayDirection }, HitResult, HitActor))
	{
		SpawnLocation = RayOrigin + RayDirection * HitResult.Distance;
	}

	EditorEngine->SpawnPlaceActor(Entry.Type, SpawnLocation);
}
