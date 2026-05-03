#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

class FWindowsWindow
{
public:
	FWindowsWindow() = default;
	~FWindowsWindow() = default;

	void Initialize(HWND InHWindow);

	HWND GetHWND() const { return HWindow; }

	float GetWidth() const { return Width; }
	float GetHeight() const { return Height; }

	void OnResized(unsigned int InWidth, unsigned int InHeight);
	void Minimize() const;
	void ToggleMaximize() const;
	void Close() const;
	void StartWindowDrag() const;
	bool IsWindowMaximized() const;
	float GetTopFrameInset() const;
	void SetTitleBarDragRegion(float X, float Y, float InWidth, float InHeight);
	void ClearTitleBarDragRegion();
	bool IsInTitleBarDragRegion(POINT ClientPoint) const;

	/** ScreenToClient 래핑 — 스크린 좌표를 클라이언트 좌표로 변환 */
	POINT ScreenToClientPoint(POINT ScreenPoint) const;

private:
	void UpdateWindowVisualStyle() const;

	HWND HWindow = nullptr;
	float Width = 0.f;
	float Height = 0.f;
	RECT TitleBarDragRegion{ 0, 0, 0, 0 };
	bool bHasTitleBarDragRegion = false;
};
