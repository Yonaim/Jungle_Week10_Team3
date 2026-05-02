#pragma once

#include "Object/FName.h"
#include "Resource/ResourceManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <string>
#include <vector>

namespace EditorPanelTitleUtils
{
	struct FPendingPanelDecoration
	{
		ImGuiWindow* Window = nullptr;
		const char* IconKey = nullptr;
		bool* VisibleFlag = nullptr;
	};

	inline ImFont*& GetPanelChromeIconFontStorage()
	{
		static ImFont* Font = nullptr;
		return Font;
	}

	inline std::vector<FPendingPanelDecoration>& GetPendingDecorations()
	{
		static std::vector<FPendingPanelDecoration> Decorations;
		return Decorations;
	}

	inline void EnsurePanelChromeIconFontLoaded()
	{
		ImFont*& Font = GetPanelChromeIconFontStorage();
		if (Font)
		{
			return;
		}

		const char* FontPath = "C:/Windows/Fonts/segmdl2.ttf";
		if (!std::filesystem::exists(FontPath))
		{
			return;
		}

		ImFontConfig FontConfig{};
		FontConfig.PixelSnapH = true;
		Font = ImGui::GetIO().Fonts->AddFontFromFileTTF(FontPath, 12.0f, &FontConfig);
	}

	inline ImFont* GetPanelChromeIconFont()
	{
		return GetPanelChromeIconFontStorage();
	}

	inline const char* GetChromeCloseGlyph()
	{
		return "\xEE\xA2\xBB";
	}

	inline void BeginPanelDecorationFrame()
	{
		GetPendingDecorations().clear();
	}

	inline FString GetEditorPathResource(const char* Key)
	{
		return FResourceManager::Get().ResolvePath(FName(Key));
	}

	inline ID3D11ShaderResourceView* GetEditorIcon(const char* Key)
	{
		if (!Key || Key[0] == '\0')
		{
			return nullptr;
		}

		if (FTextureResource* Texture = FResourceManager::Get().FindTexture(FName(Key)))
		{
			return Texture->SRV;
		}

		return FResourceManager::Get().FindLoadedTexture(GetEditorPathResource(Key)).Get();
	}

	inline std::string MakeClosablePanelTitle(const char* Title, const char* IconKey = nullptr)
	{
		const char* Prefix = GetEditorIcon(IconKey) ? "     " : "";
		return std::string(Prefix) + Title + "          ###" + Title;
	}

	inline ImRect GetPanelTitleRect()
	{
		ImGuiWindow* Window = ImGui::GetCurrentWindow();
		if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->TabBar)
		{
			ImGuiTabBar* TabBar = Window->DockNode->TabBar;
			for (int TabIndex = 0; TabIndex < TabBar->Tabs.Size; ++TabIndex)
			{
				const ImGuiTabItem& Tab = TabBar->Tabs[TabIndex];
				if (Tab.Window != Window)
				{
					continue;
				}

				const float TabMinX = TabBar->BarRect.Min.x + Tab.Offset;
				const float TabMaxX = TabMinX + Tab.Width;
				return ImRect(
					ImVec2(TabMinX, TabBar->BarRect.Min.y),
					ImVec2(TabMaxX, TabBar->BarRect.Max.y));
			}
		}
		if (Window && Window->DockTabIsVisible)
		{
			return Window->DC.DockTabItemRect;
		}
		return Window ? Window->TitleBarRect() : ImRect();
	}

	inline ImRect GetPanelTitleRect(const ImGuiWindow* Window)
	{
		if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->TabBar)
		{
			ImGuiTabBar* TabBar = Window->DockNode->TabBar;
			for (int TabIndex = 0; TabIndex < TabBar->Tabs.Size; ++TabIndex)
			{
				const ImGuiTabItem& Tab = TabBar->Tabs[TabIndex];
				if (Tab.Window != Window)
				{
					continue;
				}

				const float TabMinX = TabBar->BarRect.Min.x + Tab.Offset;
				const float TabMaxX = TabMinX + Tab.Width;
				return ImRect(
					ImVec2(TabMinX, TabBar->BarRect.Min.y),
					ImVec2(TabMaxX, TabBar->BarRect.Max.y));
			}
		}
		return Window ? Window->TitleBarRect() : ImRect();
	}

	inline ImDrawList* GetPanelTitleDrawList(ImGuiWindow* Window)
	{
		if (Window && Window->DockIsActive && Window->DockNode && Window->DockNode->HostWindow)
		{
			return Window->DockNode->HostWindow->DrawList;
		}
		return Window ? Window->DrawList : ImGui::GetForegroundDrawList();
	}

	inline void QueuePanelDecoration(const char* IconKey, bool* VisibleFlag)
	{
		ImGuiWindow* Window = ImGui::GetCurrentWindow();
		if (!Window)
		{
			return;
		}

		std::vector<FPendingPanelDecoration>& Decorations = GetPendingDecorations();
		for (FPendingPanelDecoration& Decoration : Decorations)
		{
			if (Decoration.Window != Window)
			{
				continue;
			}

			if (IconKey)
			{
				Decoration.IconKey = IconKey;
			}
			if (VisibleFlag)
			{
				Decoration.VisibleFlag = VisibleFlag;
			}
			return;
		}

		FPendingPanelDecoration Decoration;
		Decoration.Window = Window;
		Decoration.IconKey = IconKey;
		Decoration.VisibleFlag = VisibleFlag;
		Decorations.push_back(Decoration);
	}

	inline void DrawPanelTitleIcon(const char* IconKey, float IconSize = 16.0f)
	{
		(void)IconSize;
		QueuePanelDecoration(IconKey, nullptr);
	}

	inline bool DrawSmallPanelCloseButton(const char* DisplayTitle, bool& bVisible, const char* Id)
	{
		(void)DisplayTitle;
		(void)Id;
		QueuePanelDecoration(nullptr, &bVisible);
		return false;
	}

	inline void FlushPanelDecorations()
	{
		for (FPendingPanelDecoration& Decoration : GetPendingDecorations())
		{
			ImGuiWindow* Window = Decoration.Window;
			if (!Window)
			{
				continue;
			}

			const ImRect TitleRect = GetPanelTitleRect(Window);
			if (TitleRect.GetWidth() <= 0.0f || TitleRect.GetHeight() <= 0.0f)
			{
				continue;
			}

			ImDrawList* DrawList = GetPanelTitleDrawList(Window);
			DrawList->PushClipRect(TitleRect.Min, TitleRect.Max, true);

			if (ID3D11ShaderResourceView* Icon = GetEditorIcon(Decoration.IconKey))
			{
				const float IconSize = 14.0f;
				const float IconX = TitleRect.Min.x + 8.0f;
				const float IconY = TitleRect.Min.y + (TitleRect.GetHeight() - IconSize) * 0.5f;
				DrawList->AddImage(
					reinterpret_cast<ImTextureID>(Icon),
					ImVec2(IconX, IconY),
					ImVec2(IconX + IconSize, IconY + IconSize));
			}

			if (Decoration.VisibleFlag)
			{
				const float ButtonSize = (std::max)(TitleRect.GetHeight() - 8.0f, 16.0f);
				const ImVec2 ButtonPos(
					TitleRect.Max.x - ButtonSize - 6.0f,
					TitleRect.Min.y + (TitleRect.GetHeight() - ButtonSize) * 0.5f);
				const ImRect ButtonRect(ButtonPos, ImVec2(ButtonPos.x + ButtonSize, ButtonPos.y + ButtonSize));
				const ImVec2 MousePos = ImGui::GetIO().MousePos;
				const bool bHovered = ButtonRect.Contains(MousePos);
				const bool bClicked = bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

				if (bHovered)
				{
					DrawList->AddRectFilled(
						ButtonRect.Min,
						ButtonRect.Max,
						IM_COL32(66, 66, 74, 242),
						4.0f);
				}

				const ImU32 GlyphColor = bHovered ? IM_COL32(240, 240, 240, 255) : IM_COL32(190, 190, 190, 255);
				const char* Glyph = GetChromeCloseGlyph();
				if (ImFont* IconFont = GetPanelChromeIconFont())
				{
					const float FontSize = 11.0f;
					const ImVec2 GlyphSize = IconFont->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Glyph);
					DrawList->AddText(
						IconFont,
						FontSize,
						ImVec2(ButtonRect.Min.x + (ButtonSize - GlyphSize.x) * 0.5f, ButtonRect.Min.y + (ButtonSize - GlyphSize.y) * 0.5f + 0.5f),
						GlyphColor,
						Glyph);
				}

				if (bClicked)
				{
					*Decoration.VisibleFlag = false;
				}
			}

			DrawList->PopClipRect();
		}
	}
}
