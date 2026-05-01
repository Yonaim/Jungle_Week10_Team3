#include "Editor/UI/EditorPlayToolbarWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/PIE/PIETypes.h"
#include "Resource/ResourceManager.h"
#include "ImGui/imgui.h"
#include "WICTextureLoader.h"

#include <d3d11.h>

void FEditorPlayToolbarWidget::Initialize(UEditorEngine* InEditor, ID3D11Device* InDevice)
{
	Editor = InEditor;
	if (!InDevice) return;

	const FString PlayIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Play"));
	const FString StopIconPath = FResourceManager::Get().ResolvePath(FName("Editor.Icon.Stop"));

	DirectX::CreateWICTextureFromFile(
		InDevice, FPaths::ToWide(PlayIconPath).c_str(),
		nullptr, &PlayIcon);

	DirectX::CreateWICTextureFromFile(
		InDevice, FPaths::ToWide(StopIconPath).c_str(),
		nullptr, &StopIcon);
}

void FEditorPlayToolbarWidget::Release()
{
	if (PlayIcon) { PlayIcon->Release(); PlayIcon = nullptr; }
	if (StopIcon) { StopIcon->Release(); StopIcon = nullptr; }
	Editor = nullptr;
}

void FEditorPlayToolbarWidget::Render(float Width)
{
	if (!Editor) return;

	const ImVec2 CursorStart = ImGui::GetCursorScreenPos();

	// ??而?獄쏄퀗瑗?
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(
		CursorStart,
		ImVec2(CursorStart.x + Width, CursorStart.y + ToolbarHeight),
		IM_COL32(40, 40, 40, 255));

	// ??? 甕곌쑵???怨몃열???怨몄쁽 餓λ쵐釉??獄쏄퀣??
	const float ButtonPadding = (ToolbarHeight - IconSize) * 0.5f;
	ImGui::SetCursorScreenPos(ImVec2(CursorStart.x + ButtonPadding, CursorStart.y + ButtonPadding));

	const bool bPlaying = Editor->IsPlayingInEditor();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.3f));

	auto DrawIconButton = [&](const char* Id, ID3D11ShaderResourceView* Icon, const char* FallbackLabel, bool bDisabled) -> bool
	{
		if (bDisabled)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
		}
		bool bClicked = false;
		if (Icon)
		{
			bClicked = ImGui::ImageButton(Id, reinterpret_cast<ImTextureID>(Icon), ImVec2(IconSize, IconSize));
		}
		else
		{
			bClicked = ImGui::Button(FallbackLabel, ImVec2(IconSize + 8, IconSize + 8));
		}
		if (bDisabled)
		{
			ImGui::PopStyleVar();
			bClicked = false; // disabled ?怨밴묶?癒?퐣???????얜똻??
		}
		return bClicked;
	};

	if (DrawIconButton("##PIE_Play", PlayIcon, "Play", /*bDisabled=*/bPlaying))
	{
		FRequestPlaySessionParams Params;
		Editor->RequestPlaySession(Params);
	}

	ImGui::SameLine(0.0f, ButtonSpacing);

	if (DrawIconButton("##PIE_Stop", StopIcon, "Stop", /*bDisabled=*/!bPlaying))
	{
		Editor->RequestEndPlayMap();
	}

	ImGui::PopStyleColor(3);

	// ??쇱벉 ?꾩꼹?쀯㎘醫딅뮉 ??而??袁⑥삋嚥???곷선筌왖?袁⑥쨯 ?뚣끉苑?癰귣벊??
	ImGui::SetCursorScreenPos(ImVec2(CursorStart.x, CursorStart.y + ToolbarHeight));
}
