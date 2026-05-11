#include "Common/UI/DragDrop/DragSource.h"
#include "imgui.h"

void FDragSource::Render(ImVec2 InSize)
{
	RenderSource(InSize);

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(DragID.c_str(), &DragSourceInfo, sizeof(FDragSourceInfo));
		RenderSource(InSize);
		ImGui::EndDragDropSource();
	}
}
