#include "Component/UIButtonComponent.h"

#include "Input/InputManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Resource/ResourceManager.h"
#include "Serialization/Archive.h"

#include <Windows.h>

IMPLEMENT_CLASS(UIButtonComponent, UUIImageComponent)

UIButtonComponent::UIButtonComponent()
{
	bTickEnable = true;
	PrimaryComponentTick.SetTickEnabled(true);
	SetFont(FontName);
}

void UIButtonComponent::Serialize(FArchive& Ar)
{
	UUIImageComponent::Serialize(Ar);
	Ar << Label;
	Ar << FontName;
	Ar << LabelColor;
	Ar << LabelOffset;
	Ar << LabelScale;
	Ar << NormalTint;
	Ar << HoverTint;
	Ar << PressedTint;
}

void UIButtonComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UUIImageComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Label", EPropertyType::String, &Label });
	OutProps.push_back({ "Font", EPropertyType::Name, &FontName });
	OutProps.push_back({ "Label Color", EPropertyType::Color4, &LabelColor });
	OutProps.push_back({ "Label Offset", EPropertyType::Vec3, &LabelOffset, -4096.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Label Scale", EPropertyType::Float, &LabelScale, 0.1f, 10.0f, 0.05f });
	OutProps.push_back({ "Normal Tint", EPropertyType::Color4, &NormalTint });
	OutProps.push_back({ "Hover Tint", EPropertyType::Color4, &HoverTint });
	OutProps.push_back({ "Pressed Tint", EPropertyType::Color4, &PressedTint });
}

void UIButtonComponent::PostEditProperty(const char* PropertyName)
{
	UUIImageComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Font") == 0)
	{
		SetFont(FontName);
	}
}

void UIButtonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UUIImageComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bClickedThisFrame = false;

	POINT CursorPoint = FInputManager::Get().GetMousePos();
	HWND ForegroundWindow = ::GetForegroundWindow();
	if (ForegroundWindow)
	{
		::ScreenToClient(ForegroundWindow, &CursorPoint);
	}

	bHovered = IsPointInsideButton(static_cast<float>(CursorPoint.x), static_cast<float>(CursorPoint.y));
	bPressed = bHovered && FInputManager::Get().IsMouseButtonDown(FInputManager::MOUSE_LEFT);
	bClickedThisFrame = bHovered && FInputManager::Get().IsMouseButtonPressed(FInputManager::MOUSE_LEFT);
}

void UIButtonComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	if (ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV())
	{
		const FVector2 ResolvedSize = ResolveScreenSize2D();
		const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
		Scene.AddScreenQuad(
			SRV,
			ResolvedPosition,
			ResolvedSize,
			GetCurrentTint(),
			ZOrder);
	}

	if (!Label.empty())
	{
		const FVector2 ResolvedSize = ResolveScreenSize2D();
		const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
		const FFontResource* ResolvedFont = FResourceManager::Get().FindFont(FontName);
		Scene.AddScreenText(
			Label,
			FVector2(ResolvedPosition.X + LabelOffset.X, ResolvedPosition.Y + LabelOffset.Y),
			LabelScale,
			LabelColor,
			ResolvedFont ? ResolvedFont : CachedFont);
	}
}

void UIButtonComponent::SetFont(const FName& InFontName)
{
	FontName = InFontName;
	CachedFont = FResourceManager::Get().FindFont(FontName);
}

FVector4 UIButtonComponent::GetCurrentTint() const
{
	if (bPressed)
	{
		return PressedTint;
	}

	if (bHovered)
	{
		return HoverTint;
	}

	return NormalTint;
}

bool UIButtonComponent::IsPointInsideButton(float X, float Y) const
{
	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
	return X >= ResolvedPosition.X
		&& X <= ResolvedPosition.X + ResolvedSize.X
		&& Y >= ResolvedPosition.Y
		&& Y <= ResolvedPosition.Y + ResolvedSize.Y;
}
