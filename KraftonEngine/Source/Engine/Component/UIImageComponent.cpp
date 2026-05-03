#include "Component/UIImageComponent.h"

#include "Component/CanvasRootComponent.h"
#include "Component/UIScreenTextComponent.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

#include <algorithm>

IMPLEMENT_CLASS(UUIImageComponent, UBillboardComponent)

namespace
{
	constexpr const char* DefaultUIImageTexture = "Asset/Content/Texture/checker.png";
	const FVector4 SelectedUIOutlineColor(0.10f, 0.54f, 0.96f, -1.0f);

	FVector2 GetViewportSize2D()
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

	bool ResolveParentUILayoutRect(const USceneComponent* StartParent, FVector2& OutPosition, FVector2& OutSize)
	{
		for (const USceneComponent* Current = StartParent; Current != nullptr; Current = Current->GetParent())
		{
			if (const UCanvasRootComponent* CanvasRoot = dynamic_cast<const UCanvasRootComponent*>(Current))
			{
				const FVector& CanvasSize = CanvasRoot->GetCanvasSize();
				OutPosition = CanvasRoot->GetCanvasOrigin();
				OutSize = FVector2((std::max)(1.0f, CanvasSize.X), (std::max)(1.0f, CanvasSize.Y));
				return true;
			}

			if (const UUIImageComponent* ImageParent = dynamic_cast<const UUIImageComponent*>(Current))
			{
				if (ImageParent->ResolveLayoutRect(OutPosition, OutSize))
				{
					return true;
				}
				continue;
			}

			if (const UUIScreenTextComponent* TextParent = dynamic_cast<const UUIScreenTextComponent*>(Current))
			{
				if (TextParent->ResolveLayoutRect(OutPosition, OutSize))
				{
					return true;
				}
				continue;
			}
		}

		OutPosition = FVector2(0.0f, 0.0f);
		OutSize = GetViewportSize2D();
		return true;
	}
}

UUIImageComponent::UUIImageComponent()
{
	bTickEnable = false;
	SetTexturePath(DefaultUIImageTexture);
	SetCanDeleteFromDetails(true);
}

void UUIImageComponent::Serialize(FArchive& Ar)
{
	UBillboardComponent::Serialize(Ar);
	Ar << ScreenPosition;
	Ar << ScreenSize;
	Ar << bUseAnchoredLayout;
	Ar << Anchor;
	Ar << Alignment;
	Ar << AnchorOffset;
	Ar << Tint;
	Ar << ZOrder;
}

void UUIImageComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UActorComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::TextureSlot, &TextureSlot });
	OutProps.push_back({ "Screen Position", EPropertyType::Vec3, &ScreenPosition, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Screen Size", EPropertyType::Vec3, &ScreenSize, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Use Anchored Layout", EPropertyType::Bool, &bUseAnchoredLayout });
	OutProps.push_back({ "Anchor", EPropertyType::Vec3, &Anchor, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Alignment", EPropertyType::Vec3, &Alignment, 0.0f, 1.0f, 0.01f });
	OutProps.push_back({ "Anchor Offset", EPropertyType::Vec3, &AnchorOffset, -4096.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Tint", EPropertyType::Color4, &Tint });
	OutProps.push_back({ "Z Order", EPropertyType::Int, &ZOrder });
}

void UUIImageComponent::PostEditProperty(const char* PropertyName)
{
	UBillboardComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0 && (TextureSlot.Path.empty() || TextureSlot.Path == "None"))
	{
		SetTexturePath(DefaultUIImageTexture);
	}
	else if (strcmp(PropertyName, "Screen Size") == 0)
	{
		SetScreenSize(ScreenSize);
	}
}

void UUIImageComponent::ContributeVisuals(FScene& Scene) const
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
			Tint,
			ZOrder);
	}
}

void UUIImageComponent::ContributeSelectedVisuals(FScene& Scene) const
{
	if (!IsVisible() || Scene.GetSelectedComponent() != this)
	{
		return;
	}

	ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV();
	if (!SRV)
	{
		return;
	}

	constexpr float OutlineThickness = 3.0f;
	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
	const float X = ResolvedPosition.X;
	const float Y = ResolvedPosition.Y;
	const float W = ResolvedSize.X;
	const float H = ResolvedSize.Y;
	const int32 OutlineZ = ZOrder + 1000;

	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y - OutlineThickness), FVector2(W + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y + H), FVector2(W + OutlineThickness * 2.0f, OutlineThickness), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X - OutlineThickness, Y), FVector2(OutlineThickness, H), SelectedUIOutlineColor, OutlineZ);
	Scene.AddScreenQuad(nullptr, FVector2(X + W, Y), FVector2(OutlineThickness, H), SelectedUIOutlineColor, OutlineZ);
}

bool UUIImageComponent::HitTestUIScreenPoint(float X, float Y) const
{
	if (!IsVisible())
	{
		return false;
	}

	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
	return X >= ResolvedPosition.X
		&& X <= ResolvedPosition.X + ResolvedSize.X
		&& Y >= ResolvedPosition.Y
		&& Y <= ResolvedPosition.Y + ResolvedSize.Y;
}

void UUIImageComponent::SetScreenSize(const FVector& InScreenSize)
{
	ScreenSize.X = (std::max)(1.0f, InScreenSize.X);
	ScreenSize.Y = (std::max)(1.0f, InScreenSize.Y);
	ScreenSize.Z = InScreenSize.Z;

	Width = ScreenSize.X;
	Height = ScreenSize.Y;
}

bool UUIImageComponent::SetTexturePath(const FString& InTexturePath)
{
	TextureSlot.Path = InTexturePath.empty() ? FString(DefaultUIImageTexture) : InTexturePath;
	return EnsureTextureLoaded();
}

bool UUIImageComponent::EnsureTextureLoaded()
{
	if (TextureSlot.Path.empty() || TextureSlot.Path == "None")
	{
		TextureSlot.Path = DefaultUIImageTexture;
	}

	if (ResolveTextureFromPath(TextureSlot.Path))
	{
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	if (UTexture2D* LoadedTexture = UTexture2D::LoadFromFile(TextureSlot.Path, Device))
	{
		SetTexture(LoadedTexture);
		return true;
	}

	return false;
}

ID3D11ShaderResourceView* UUIImageComponent::GetResolvedTextureSRV() const
{
	if (Texture && Texture->IsLoaded())
	{
		return Texture->GetSRV();
	}

	return nullptr;
}

bool UUIImageComponent::ResolveLayoutRect(FVector2& OutPosition, FVector2& OutSize) const
{
	OutSize = ResolveScreenSize2D();
	OutPosition = ResolveScreenPosition(OutSize);
	return true;
}

FVector2 UUIImageComponent::GetResolvedScreenPosition() const
{
	const FVector2 ResolvedSize = ResolveScreenSize2D();
	return ResolveScreenPosition(ResolvedSize);
}

FVector2 UUIImageComponent::GetResolvedScreenSize() const
{
	return ResolveScreenSize2D();
}

FVector2 UUIImageComponent::ResolveScreenPosition(const FVector2& ElementSize) const
{
	if (!bUseAnchoredLayout)
	{
		return FVector2(ScreenPosition.X, ScreenPosition.Y);
	}

	FVector2 ParentPosition(0.0f, 0.0f);
	FVector2 ParentSize = GetViewportSize2D();
	ResolveParentUILayoutRect(GetParent(), ParentPosition, ParentSize);

	const float ClampedAnchorX = (std::clamp)(Anchor.X, 0.0f, 1.0f);
	const float ClampedAnchorY = (std::clamp)(Anchor.Y, 0.0f, 1.0f);
	const float ClampedAlignmentX = (std::clamp)(Alignment.X, 0.0f, 1.0f);
	const float ClampedAlignmentY = (std::clamp)(Alignment.Y, 0.0f, 1.0f);

	return FVector2(
		ParentPosition.X + ParentSize.X * ClampedAnchorX + AnchorOffset.X - ElementSize.X * ClampedAlignmentX,
		ParentPosition.Y + ParentSize.Y * ClampedAnchorY + AnchorOffset.Y - ElementSize.Y * ClampedAlignmentY);
}

FVector2 UUIImageComponent::ResolveScreenSize2D() const
{
	return FVector2(ScreenSize.X, ScreenSize.Y);
}
