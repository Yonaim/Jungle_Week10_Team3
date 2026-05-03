#include "Component/UWindowPanelComponent.h"

#include "Object/ObjectFactory.h"
#include "Engine/Runtime/Engine.h"
#include "Platform/Paths.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"
#include "Texture/Texture2D.h"

#include <algorithm>
#include <filesystem>

IMPLEMENT_CLASS(UNineSlicePanelComponent, UUIImageComponent)
IMPLEMENT_CLASS(UWindowPanelComponent, UNineSlicePanelComponent)
HIDE_FROM_COMPONENT_LIST(UWindowPanelComponent)

namespace
{
	void FitBordersToTarget(float TargetSize, float& StartSize, float& EndSize)
	{
		StartSize = (std::max)(0.0f, StartSize);
		EndSize = (std::max)(0.0f, EndSize);

		const float Total = StartSize + EndSize;
		if (Total > TargetSize && Total > 0.0f)
		{
			const float Scale = TargetSize / Total;
			StartSize *= Scale;
			EndSize *= Scale;
		}
	}
}

UNineSlicePanelComponent::UNineSlicePanelComponent()
{
	Slice = FVector4(8.0f, 24.0f, 8.0f, 8.0f);
}

void UNineSlicePanelComponent::Serialize(FArchive& Ar)
{
	UUIImageComponent::Serialize(Ar);
	Ar << Slice;
	Ar << bDrawBorder;
	Ar << bDrawCenter;
	Ar << TopLeftTextureSlot.Path;
	Ar << TopTextureSlot.Path;
	Ar << TopRightTextureSlot.Path;
	Ar << LeftTextureSlot.Path;
	Ar << CenterTextureSlot.Path;
	Ar << RightTextureSlot.Path;
	Ar << BottomLeftTextureSlot.Path;
	Ar << BottomTextureSlot.Path;
	Ar << BottomRightTextureSlot.Path;
}

void UNineSlicePanelComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UUIImageComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Nine Slice Border", EPropertyType::Vec4, &Slice, 0.0f, 4096.0f, 1.0f });
	OutProps.push_back({ "Draw Border", EPropertyType::Bool, &bDrawBorder });
	OutProps.push_back({ "Draw Center", EPropertyType::Bool, &bDrawCenter });
	OutProps.push_back({ "Top Left Texture", EPropertyType::TextureSlot, &TopLeftTextureSlot });
	OutProps.push_back({ "Top Texture", EPropertyType::TextureSlot, &TopTextureSlot });
	OutProps.push_back({ "Top Right Texture", EPropertyType::TextureSlot, &TopRightTextureSlot });
	OutProps.push_back({ "Left Texture", EPropertyType::TextureSlot, &LeftTextureSlot });
	OutProps.push_back({ "Center Texture", EPropertyType::TextureSlot, &CenterTextureSlot });
	OutProps.push_back({ "Right Texture", EPropertyType::TextureSlot, &RightTextureSlot });
	OutProps.push_back({ "Bottom Left Texture", EPropertyType::TextureSlot, &BottomLeftTextureSlot });
	OutProps.push_back({ "Bottom Texture", EPropertyType::TextureSlot, &BottomTextureSlot });
	OutProps.push_back({ "Bottom Right Texture", EPropertyType::TextureSlot, &BottomRightTextureSlot });
}

void UNineSlicePanelComponent::PostEditProperty(const char* PropertyName)
{
	UUIImageComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Slice") == 0
		|| strcmp(PropertyName, "Nine Slice Border") == 0
		|| strcmp(PropertyName, "Texture") == 0)
	{
		ClampSlice();
	}

	if (strcmp(PropertyName, "Top Left Texture") == 0)
	{
		ResolveOptionalTextureSlot(TopLeftTextureSlot, TopLeftTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Top Texture") == 0)
	{
		ResolveOptionalTextureSlot(TopTextureSlot, TopTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Top Right Texture") == 0)
	{
		ResolveOptionalTextureSlot(TopRightTextureSlot, TopRightTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Left Texture") == 0)
	{
		ResolveOptionalTextureSlot(LeftTextureSlot, LeftTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Center Texture") == 0)
	{
		ResolveOptionalTextureSlot(CenterTextureSlot, CenterTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Right Texture") == 0)
	{
		ResolveOptionalTextureSlot(RightTextureSlot, RightTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Bottom Left Texture") == 0)
	{
		ResolveOptionalTextureSlot(BottomLeftTextureSlot, BottomLeftTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Bottom Texture") == 0)
	{
		ResolveOptionalTextureSlot(BottomTextureSlot, BottomTexture);
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Bottom Right Texture") == 0)
	{
		ResolveOptionalTextureSlot(BottomRightTextureSlot, BottomRightTexture);
		MarkRenderStateDirty();
	}
}

void UNineSlicePanelComponent::ContributeVisuals(FScene& Scene) const
{
	if (!IsVisible())
	{
		return;
	}

	ID3D11ShaderResourceView* SRV = GetResolvedTextureSRV();
	UTexture2D* Texture2D = GetTexture();
	if (!SRV || !Texture2D || !Texture2D->IsLoaded())
	{
		return;
	}

	const float TextureWidth = static_cast<float>(Texture2D->GetWidth());
	const float TextureHeight = static_cast<float>(Texture2D->GetHeight());
	if (TextureWidth <= 0.0f || TextureHeight <= 0.0f)
	{
		return;
	}

	float SliceLeft = (std::clamp)(Slice.X, 0.0f, TextureWidth);
	float SliceTop = (std::clamp)(Slice.Y, 0.0f, TextureHeight);
	float SliceRight = (std::clamp)(Slice.Z, 0.0f, TextureWidth - SliceLeft);
	float SliceBottom = (std::clamp)(Slice.W, 0.0f, TextureHeight - SliceTop);

	const FVector2 ResolvedSize = ResolveScreenSize2D();
	const FVector2 ResolvedPosition = ResolveScreenPosition(ResolvedSize);
	float LeftWidth = SliceLeft;
	float RightWidth = SliceRight;
	float TopHeight = SliceTop;
	float BottomHeight = SliceBottom;

	FitBordersToTarget(ResolvedSize.X, LeftWidth, RightWidth);
	FitBordersToTarget(ResolvedSize.Y, TopHeight, BottomHeight);

	const float CenterWidth = (std::max)(0.0f, ResolvedSize.X - LeftWidth - RightWidth);
	const float CenterHeight = (std::max)(0.0f, ResolvedSize.Y - TopHeight - BottomHeight);

	const float X0 = ResolvedPosition.X;
	const float X1 = X0 + LeftWidth;
	const float X2 = X0 + ResolvedSize.X - RightWidth;
	const float Y0 = ResolvedPosition.Y;
	const float Y1 = Y0 + TopHeight;
	const float Y2 = Y0 + ResolvedSize.Y - BottomHeight;

	const float U0 = 0.0f;
	const float U1 = SliceLeft / TextureWidth;
	const float U2 = 1.0f - (SliceRight / TextureWidth);
	const float U3 = 1.0f;
	const float V0 = 0.0f;
	const float V1 = SliceTop / TextureHeight;
	const float V2 = 1.0f - (SliceBottom / TextureHeight);
	const float V3 = 1.0f;

	if (bDrawBorder)
	{
		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(TopLeftTextureSlot, TopLeftTexture))
			AddPanelQuad(Scene, OverrideSRV, X0, Y0, LeftWidth, TopHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X0, Y0, LeftWidth, TopHeight, U0, V0, U1, V1);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(TopTextureSlot, TopTexture))
			AddPanelQuad(Scene, OverrideSRV, X1, Y0, CenterWidth, TopHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X1, Y0, CenterWidth, TopHeight, U1, V0, U2, V1);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(TopRightTextureSlot, TopRightTexture))
			AddPanelQuad(Scene, OverrideSRV, X2, Y0, RightWidth, TopHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X2, Y0, RightWidth, TopHeight, U2, V0, U3, V1);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(LeftTextureSlot, LeftTexture))
			AddPanelQuad(Scene, OverrideSRV, X0, Y1, LeftWidth, CenterHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X0, Y1, LeftWidth, CenterHeight, U0, V1, U1, V2);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(RightTextureSlot, RightTexture))
			AddPanelQuad(Scene, OverrideSRV, X2, Y1, RightWidth, CenterHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X2, Y1, RightWidth, CenterHeight, U2, V1, U3, V2);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(BottomLeftTextureSlot, BottomLeftTexture))
			AddPanelQuad(Scene, OverrideSRV, X0, Y2, LeftWidth, BottomHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X0, Y2, LeftWidth, BottomHeight, U0, V2, U1, V3);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(BottomTextureSlot, BottomTexture))
			AddPanelQuad(Scene, OverrideSRV, X1, Y2, CenterWidth, BottomHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X1, Y2, CenterWidth, BottomHeight, U1, V2, U2, V3);

		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(BottomRightTextureSlot, BottomRightTexture))
			AddPanelQuad(Scene, OverrideSRV, X2, Y2, RightWidth, BottomHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X2, Y2, RightWidth, BottomHeight, U2, V2, U3, V3);
	}

	if (bDrawCenter)
	{
		if (ID3D11ShaderResourceView* OverrideSRV = GetOptionalTextureSRV(CenterTextureSlot, CenterTexture))
			AddPanelQuad(Scene, OverrideSRV, X1, Y1, CenterWidth, CenterHeight, 0.0f, 0.0f, 1.0f, 1.0f);
		else
			AddPanelQuad(Scene, SRV, X1, Y1, CenterWidth, CenterHeight, U1, V1, U2, V2);
	}
}

void UNineSlicePanelComponent::ClampSlice()
{
	Slice.X = (std::max)(0.0f, Slice.X);
	Slice.Y = (std::max)(0.0f, Slice.Y);
	Slice.Z = (std::max)(0.0f, Slice.Z);
	Slice.W = (std::max)(0.0f, Slice.W);

	UTexture2D* Texture2D = GetTexture();
	if (!Texture2D || !Texture2D->IsLoaded())
	{
		return;
	}

	const float TextureWidth = static_cast<float>(Texture2D->GetWidth());
	const float TextureHeight = static_cast<float>(Texture2D->GetHeight());
	Slice.X = (std::clamp)(Slice.X, 0.0f, TextureWidth);
	Slice.Y = (std::clamp)(Slice.Y, 0.0f, TextureHeight);
	Slice.Z = (std::clamp)(Slice.Z, 0.0f, TextureWidth - Slice.X);
	Slice.W = (std::clamp)(Slice.W, 0.0f, TextureHeight - Slice.Y);
}

bool UNineSlicePanelComponent::ResolveOptionalTextureSlot(FTextureSlot& Slot, UTexture2D*& LoadedTexture)
{
	if (Slot.Path.empty() || Slot.Path == "None")
	{
		Slot.Path = "None";
		LoadedTexture = nullptr;
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device)
	{
		return false;
	}

	const FString ResolvedPath = FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(Slot.Path)).lexically_normal().wstring());
	if (UTexture2D* Texture2D = UTexture2D::LoadFromFile(ResolvedPath, Device))
	{
		Slot.Path = ResolvedPath;
		LoadedTexture = Texture2D;
		return true;
	}

	return false;
}

ID3D11ShaderResourceView* UNineSlicePanelComponent::GetOptionalTextureSRV(const FTextureSlot& Slot, UTexture2D* LoadedTexture) const
{
	if (Slot.Path.empty() || Slot.Path == "None" || !LoadedTexture || !LoadedTexture->IsLoaded())
	{
		return nullptr;
	}

	return LoadedTexture->GetSRV();
}

void UNineSlicePanelComponent::AddPanelQuad(FScene& Scene, ID3D11ShaderResourceView* SRV, float X, float Y, float Width, float Height,
	float U0, float V0, float U1, float V1) const
{
	if (Width <= 0.0f || Height <= 0.0f)
	{
		return;
	}

	Scene.AddScreenQuad(
		SRV,
		FVector2(X, Y),
		FVector2(Width, Height),
		Tint,
		ZOrder,
		FVector2(U0, V0),
		FVector2(U1, V1));
}
