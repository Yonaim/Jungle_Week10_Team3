#include "Component/UIBackgroundComponent.h"

#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"

#include <algorithm>
#include <cstring>

IMPLEMENT_CLASS(UUIBackgroundComponent, UUIImageComponent)

namespace
{
	const FVector4 SelectedUIOutlineColor(0.10f, 0.54f, 0.96f, -1.0f);
}

UUIBackgroundComponent::UUIBackgroundComponent()
{
	SetScreenPosition(FVector(0.0f, 0.0f, 0.0f));
	SetScreenSize(FVector(1920.0f, 1080.0f, 0.0f));
	SetAnchoredLayoutEnabled(false);
	SetZOrder(-10000);
}

void UUIBackgroundComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::TextureSlot, &TextureSlot });
	OutProps.push_back({ "Tint", EPropertyType::Color4, &Tint });
	OutProps.push_back({ "Z Order", EPropertyType::Int, &ZOrder });
}

void UUIBackgroundComponent::PostEditProperty(const char* PropertyName)
{
	UUIImageComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0 && (TextureSlot.Path.empty() || TextureSlot.Path == "None"))
	{
		SetTexturePath("");
	}
}

void UUIBackgroundComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	if (ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV())
	{
		Scene.AddScreenQuad(SRV, FVector2(0.0f, 0.0f), GetViewportSize2D(), Tint, ZOrder);
	}
}

void UUIBackgroundComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	if (!IsVisible() || Scene.GetSelectedComponent() != this)
	{
		return;
	}

	if (!GetResolvedTextureSRV())
	{
		return;
	}

	constexpr float OutlineThickness = 3.0f;
	const FVector2 Size = GetViewportSize2D();
	const int32 OutlineZ = ZOrder + 1000;

	Scene.AddScreenQuad(nullptr, FVector2(-OutlineThickness, -OutlineThickness), FVector2(Size.X + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(-OutlineThickness, Size.Y), FVector2(Size.X + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(-OutlineThickness, 0.0f), FVector2(OutlineThickness, Size.Y), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(Size.X, 0.0f), FVector2(OutlineThickness, Size.Y), SelectedUIOutlineColor, OutlineZ);
}

bool UUIBackgroundComponent::HitTestUIScreenPoint(float X, float Y) const
{
	if (!IsVisible())
	{
		return false;
	}

	const FVector2 Size = GetViewportSize2D();
	return X >= 0.0f && X <= Size.X && Y >= 0.0f && Y <= Size.Y;
}

FVector2 UUIBackgroundComponent::GetViewportSize2D() const
{
	FVector2 ViewportSize(1920.0f, 1080.0f);
	if (GEngine)
	{
		if (FWindowsWindow* Window = GEngine->GetWindow())
		{
			ViewportSize.X = (std::max)(1.0f, Window->GetWidth());
			ViewportSize.Y = (std::max)(1.0f, Window->GetHeight());
		}
	}

	return ViewportSize;
}
