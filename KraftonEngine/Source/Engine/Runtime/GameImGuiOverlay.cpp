#include "Engine/Runtime/GameImGuiOverlay.h"

#include "Engine/Runtime/WindowsWindow.h"
#include "UI/SWindow.h"
#include "Render/Pipeline/Renderer.h"
#include "Platform/Paths.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "SimpleJSON/json.hpp"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
	constexpr ImVec4 ScorePopupSurface = ImVec4(42.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 0.98f);
	constexpr ImVec4 ScorePopupTitle = ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 44.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScorePopupBorder = ImVec4(58.0f / 255.0f, 58.0f / 255.0f, 58.0f / 255.0f, 1.0f);
	constexpr ImVec4 ScorePopupDim = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
}

bool FGameImGuiOverlay::IsAlphabetCharacter(char Character)
{
	return (Character >= 'A' && Character <= 'Z') || (Character >= 'a' && Character <= 'z');
}

void FGameImGuiOverlay::Initialize(FWindowsWindow* InWindow, FRenderer& InRenderer)
{
	if (bInitialized || !InWindow)
	{
		return;
	}

	ImGui::CreateContext();
	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	IO.IniFilename = nullptr;
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(
		InRenderer.GetFD3DDevice().GetDevice(),
		InRenderer.GetFD3DDevice().GetDeviceContext());

	bInitialized = true;
}

void FGameImGuiOverlay::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	ScoreSavePopup = FScoreSavePopupState();
	MessagePopup = FMessagePopupState();
	ScoreboardPopup = FScoreboardPopupState();
	bInitialized = false;
}

void FGameImGuiOverlay::Render(FRenderer& InRenderer)
{
	(void)InRenderer;
	if (!bInitialized)
	{
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	RenderScoreSavePopup();
	RenderMessagePopup();
	RenderScoreboardPopup();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FGameImGuiOverlay::RenderWithinCurrentFrame(const FRect* AnchorRect)
{
	RenderScoreSavePopup(AnchorRect);
	RenderMessagePopup(AnchorRect);
	RenderScoreboardPopup(AnchorRect);
}

void FGameImGuiOverlay::OpenScoreSavePopup(int32 InScore)
{
	ScoreSavePopup.Score = InScore;
	ScoreSavePopup.bSubmitted = false;
	ScoreSavePopup.PendingNickname.clear();
	ScoreSavePopup.bRequestOpen = true;
	ScoreSavePopup.bPopupOpen = true;
	ScoreSavePopup.bFocusInput = true;
	ResetNicknameBuffer();
}

bool FGameImGuiOverlay::ConsumeScoreSavePopupResult(FString& OutNickname)
{
	if (!ScoreSavePopup.bSubmitted)
	{
		return false;
	}

	OutNickname = ScoreSavePopup.PendingNickname;
	ScoreSavePopup.PendingNickname.clear();
	ScoreSavePopup.bSubmitted = false;
	return !OutNickname.empty();
}

void FGameImGuiOverlay::OpenMessagePopup(const FString& InMessage)
{
	MessagePopup.Message = InMessage;
	MessagePopup.bConfirmed = false;
	MessagePopup.bRequestOpen = true;
	MessagePopup.bPopupOpen = true;
}

bool FGameImGuiOverlay::ConsumeMessagePopupConfirmed()
{
	if (!MessagePopup.bConfirmed)
	{
		return false;
	}

	MessagePopup.bConfirmed = false;
	return true;
}

void FGameImGuiOverlay::OpenScoreboardPopup(const FString& InFilePath)
{
	ScoreboardPopup.FilePath = InFilePath;
	ScoreboardPopup.ErrorMessage.clear();
	ScoreboardPopup.Entries.clear();
	ScoreboardPopup.bRequestOpen = true;
	ScoreboardPopup.bPopupOpen = true;
}

bool FGameImGuiOverlay::IsScoreSavePopupOpen() const
{
	return
		ScoreSavePopup.bPopupOpen ||
		ScoreSavePopup.bRequestOpen ||
		MessagePopup.bPopupOpen ||
		MessagePopup.bRequestOpen ||
		ScoreboardPopup.bPopupOpen ||
		ScoreboardPopup.bRequestOpen;
}

void FGameImGuiOverlay::ResetNicknameBuffer()
{
	std::memset(ScoreSavePopup.NicknameBuffer, 0, sizeof(ScoreSavePopup.NicknameBuffer));
}

void FGameImGuiOverlay::SanitizeNicknameBuffer()
{
	char Sanitized[7] = {};
	int32 WriteIndex = 0;
	for (int32 ReadIndex = 0; ReadIndex < 6 && ScoreSavePopup.NicknameBuffer[ReadIndex] != '\0'; ++ReadIndex)
	{
		char Character = ScoreSavePopup.NicknameBuffer[ReadIndex];
		if (!IsAlphabetCharacter(Character))
		{
			continue;
		}

		Sanitized[WriteIndex++] = static_cast<char>(std::toupper(static_cast<unsigned char>(Character)));
		if (WriteIndex >= 6)
		{
			break;
		}
	}

	std::memcpy(ScoreSavePopup.NicknameBuffer, Sanitized, sizeof(Sanitized));
}

void FGameImGuiOverlay::RenderScoreSavePopup(const FRect* AnchorRect)
{
	if (ScoreSavePopup.bRequestOpen)
	{
		ImGui::OpenPopup("Save Score");
		ScoreSavePopup.bRequestOpen = false;
		ScoreSavePopup.bPopupOpen = true;
		ScoreSavePopup.bFocusInput = true;
	}

	ImVec2 PopupCenter;
	if (AnchorRect && AnchorRect->Width > 0.0f && AnchorRect->Height > 0.0f)
	{
		PopupCenter = ImVec2(
			AnchorRect->X + AnchorRect->Width * 0.5f,
			AnchorRect->Y + AnchorRect->Height * 0.5f);
	}
	else
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		PopupCenter = MainViewport->GetCenter();
	}

	ImGui::SetNextWindowPos(PopupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ScorePopupSurface);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_Border, ScorePopupBorder);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ScorePopupDim);

	if (ImGui::BeginPopupModal("Save Score", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Final Score: %d", ScoreSavePopup.Score);
		ImGui::Spacing();
		ImGui::TextUnformatted("Nickname");
		if (ScoreSavePopup.bFocusInput)
		{
			ImGui::SetKeyboardFocusHere();
			ScoreSavePopup.bFocusInput = false;
		}

		ImGui::InputText("##Nickname", ScoreSavePopup.NicknameBuffer, IM_ARRAYSIZE(ScoreSavePopup.NicknameBuffer));
		SanitizeNicknameBuffer();
		ImGui::TextDisabled("Alphabet only, max 6 characters");
		ImGui::Spacing();

		const bool bCanSave = ScoreSavePopup.NicknameBuffer[0] != '\0';
		if (!bCanSave)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Save", ImVec2(140.0f, 0.0f)))
		{
			ScoreSavePopup.PendingNickname = ScoreSavePopup.NicknameBuffer;
			ScoreSavePopup.bSubmitted = true;
			ScoreSavePopup.bPopupOpen = false;
			ResetNicknameBuffer();
			ImGui::CloseCurrentPopup();
		}

		if (!bCanSave)
		{
			ImGui::EndDisabled();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(140.0f, 0.0f)))
		{
			ScoreSavePopup.bPopupOpen = false;
			ResetNicknameBuffer();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(6);
	ImGui::PopStyleVar(2);

	if (ScoreSavePopup.bPopupOpen && !ImGui::IsPopupOpen("Save Score"))
	{
		ScoreSavePopup.bPopupOpen = false;
		ResetNicknameBuffer();
	}
}

void FGameImGuiOverlay::RenderMessagePopup(const FRect* AnchorRect)
{
	if (MessagePopup.bRequestOpen)
	{
		ImGui::OpenPopup("Notice");
		MessagePopup.bRequestOpen = false;
		MessagePopup.bPopupOpen = true;
	}

	ImVec2 PopupCenter;
	if (AnchorRect && AnchorRect->Width > 0.0f && AnchorRect->Height > 0.0f)
	{
		PopupCenter = ImVec2(
			AnchorRect->X + AnchorRect->Width * 0.5f,
			AnchorRect->Y + AnchorRect->Height * 0.5f);
	}
	else
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		PopupCenter = MainViewport->GetCenter();
	}

	ImGui::SetNextWindowPos(PopupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ScorePopupSurface);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_Border, ScorePopupBorder);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ScorePopupDim);

	if (ImGui::BeginPopupModal("Notice", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("%s", MessagePopup.Message.c_str());
		ImGui::Spacing();

		if (ImGui::Button("OK", ImVec2(140.0f, 0.0f)))
		{
			MessagePopup.bConfirmed = true;
			MessagePopup.bPopupOpen = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(6);
	ImGui::PopStyleVar(2);

	if (MessagePopup.bPopupOpen && !ImGui::IsPopupOpen("Notice"))
	{
		MessagePopup.bPopupOpen = false;
	}
}

void FGameImGuiOverlay::LoadScoreboardEntries()
{
	ScoreboardPopup.ErrorMessage.clear();
	ScoreboardPopup.Entries.clear();

	const std::filesystem::path InputPath = FPaths::ToWide(ScoreboardPopup.FilePath);
	const std::filesystem::path AbsolutePath = InputPath.is_absolute()
		? InputPath.lexically_normal()
		: (std::filesystem::path(FPaths::RootDir()) / InputPath).lexically_normal();

	std::ifstream File(AbsolutePath);
	if (!File.is_open())
	{
		ScoreboardPopup.ErrorMessage = "Scoreboard file not found.";
		return;
	}

	const FString Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
	if (Content.empty())
	{
		ScoreboardPopup.ErrorMessage = "Scoreboard file is empty.";
		return;
	}

	json::JSON Root = json::JSON::Load(Content);
	json::JSON EntriesJson = Root.hasKey("entries") ? Root["entries"] : Root;
	if (EntriesJson.JSONType() != json::JSON::Class::Array)
	{
		ScoreboardPopup.ErrorMessage = "Scoreboard data is invalid.";
		return;
	}

	for (auto& EntryJson : EntriesJson.ArrayRange())
	{
		FScoreboardEntry Entry;
		Entry.Nickname = EntryJson.hasKey("nickname") ? EntryJson["nickname"].ToString() : "";
		Entry.Score = EntryJson.hasKey("score") ? EntryJson["score"].ToInt() : 0;
		Entry.Logs = EntryJson.hasKey("logs") ? EntryJson["logs"].ToInt() : 0;
		Entry.HotfixCount = EntryJson.hasKey("hotfix_count") ? EntryJson["hotfix_count"].ToInt() : 0;
		Entry.CrashDumpAnalysisCount = EntryJson.hasKey("crash_dump_analysis_count") ? EntryJson["crash_dump_analysis_count"].ToInt() : 0;
		Entry.MaxDepthM = EntryJson.hasKey("max_depth_m") ? EntryJson["max_depth_m"].ToInt() : 0;
		Entry.CoachRank = EntryJson.hasKey("coach_rank") ? EntryJson["coach_rank"].ToString() : "";
		ScoreboardPopup.Entries.push_back(std::move(Entry));
	}
}

void FGameImGuiOverlay::RenderScoreboardPopup(const FRect* AnchorRect)
{
	if (ScoreboardPopup.bRequestOpen)
	{
		LoadScoreboardEntries();
		ImGui::OpenPopup("Scoreboard");
		ScoreboardPopup.bRequestOpen = false;
		ScoreboardPopup.bPopupOpen = true;
	}

	ImVec2 PopupCenter;
	if (AnchorRect && AnchorRect->Width > 0.0f && AnchorRect->Height > 0.0f)
	{
		PopupCenter = ImVec2(
			AnchorRect->X + AnchorRect->Width * 0.5f,
			AnchorRect->Y + AnchorRect->Height * 0.5f);
	}
	else
	{
		ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		PopupCenter = MainViewport->GetCenter();
	}

	ImGui::SetNextWindowPos(PopupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(960.0f, 560.0f), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ScorePopupSurface);
	ImGui::PushStyleColor(ImGuiCol_TitleBg, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ScorePopupTitle);
	ImGui::PushStyleColor(ImGuiCol_Border, ScorePopupBorder);
	ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ScorePopupDim);

	if (ImGui::BeginPopupModal("Scoreboard", nullptr, ImGuiWindowFlags_NoResize))
	{
		ImGui::TextUnformatted("SCOREBOARD");
		ImGui::Separator();

		if (!ScoreboardPopup.ErrorMessage.empty())
		{
			ImGui::TextWrapped("%s", ScoreboardPopup.ErrorMessage.c_str());
		}
		else if (ScoreboardPopup.Entries.empty())
		{
			ImGui::TextUnformatted("No saved scores yet.");
		}
		else if (ImGui::BeginTable("##ScoreboardTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 90.0f);
			ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableSetupColumn("Logs", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Hotfix", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Crash", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Depth / Rank");
			ImGui::TableHeadersRow();

			for (size_t Index = 0; Index < ScoreboardPopup.Entries.size(); ++Index)
			{
				const FScoreboardEntry& Entry = ScoreboardPopup.Entries[Index];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%d", static_cast<int>(Index + 1));
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(Entry.Nickname.c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%d", Entry.Score);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%d", Entry.Logs);
				ImGui::TableSetColumnIndex(4);
				ImGui::Text("%d", Entry.HotfixCount);
				ImGui::TableSetColumnIndex(5);
				ImGui::Text("%d", Entry.CrashDumpAnalysisCount);
				ImGui::TableSetColumnIndex(6);
				ImGui::Text("%dm / %s", Entry.MaxDepthM, Entry.CoachRank.c_str());
			}

			ImGui::EndTable();
		}

		ImGui::Spacing();
		if (ImGui::Button("Close", ImVec2(140.0f, 0.0f)))
		{
			ScoreboardPopup.bPopupOpen = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(6);
	ImGui::PopStyleVar(2);

	if (ScoreboardPopup.bPopupOpen && !ImGui::IsPopupOpen("Scoreboard"))
	{
		ScoreboardPopup.bPopupOpen = false;
	}
}
