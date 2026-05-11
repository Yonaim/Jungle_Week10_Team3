#include "LevelEditor/UI/Panels/LevelControlPanel.h"
#include "Common/UI/EditorPanel.h"
#include "EditorEngine.h"
#include "ImGui/imgui.h"
#include "Component/CameraComponent.h"

void FLevelControlPanel::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	FEditorPanelDesc PanelDesc;
	PanelDesc.DisplayName = "Control Panel";
	PanelDesc.StableId = "LevelControlPanel";
	PanelDesc.IconKey = "Editor.Icon.Panel.Settings";
	if (!FEditorPanel::Begin(PanelDesc))
	{
		FEditorPanel::End();
		return;
	}

	ImGui::TextUnformatted("Camera controls moved to the viewport toolbar.");
	ImGui::TextUnformatted("Use the camera button next to Show.");

	FEditorPanel::End();
}
