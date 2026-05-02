#include "Engine/Runtime/WindowsWindow.h"

void FWindowsWindow::Initialize(HWND InHWindow)
{
	HWindow = InHWindow;

	RECT Rect;
	GetClientRect(HWindow, &Rect);
	Width = static_cast<float>(Rect.right - Rect.left);
	Height = static_cast<float>(Rect.bottom - Rect.top);
}

void FWindowsWindow::OnResized(unsigned int InWidth, unsigned int InHeight)
{
	Width = static_cast<float>(InWidth);
	Height = static_cast<float>(InHeight);
}

void FWindowsWindow::Minimize() const
{
	if (HWindow)
	{
		ShowWindow(HWindow, SW_MINIMIZE);
	}
}

void FWindowsWindow::ToggleMaximize() const
{
	if (!HWindow)
	{
		return;
	}

	ShowWindow(HWindow, IsZoomed(HWindow) ? SW_RESTORE : SW_MAXIMIZE);
}

void FWindowsWindow::Close() const
{
	if (HWindow)
	{
		PostMessage(HWindow, WM_CLOSE, 0, 0);
	}
}

void FWindowsWindow::StartWindowDrag() const
{
	if (!HWindow)
	{
		return;
	}

	ReleaseCapture();
	// Avoid re-entering the Win32 move loop while we are still inside the current ImGui frame.
	PostMessage(HWindow, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

bool FWindowsWindow::IsWindowMaximized() const
{
	return HWindow && IsZoomed(HWindow);
}

float FWindowsWindow::GetTopFrameInset() const
{
	if (!HWindow || IsWindowMaximized())
	{
		return 0.0f;
	}

	const UINT Dpi = GetDpiForWindow(HWindow);
	const int FrameY = GetSystemMetricsForDpi(SM_CYFRAME, Dpi);
	const int PaddedBorder = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, Dpi);
	return static_cast<float>(FrameY + PaddedBorder);
}

POINT FWindowsWindow::ScreenToClientPoint(POINT ScreenPoint) const
{
	ScreenToClient(HWindow, &ScreenPoint);
	return ScreenPoint;
}
