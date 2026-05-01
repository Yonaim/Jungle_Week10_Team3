#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Proxy/BillboardSceneProxy.h"
#include "Serialization/Archive.h"
#include "Object/ObjectFactory.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Resource/ResourceManager.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

#include <cstring>

IMPLEMENT_CLASS(UBillboardComponent, UPrimitiveComponent)

namespace
{
FString ResolveLegacyBillboardTexturePath(const FString& InPath)
{
	const FString FileName = std::filesystem::path(InPath).filename().string();

	if (FileName == "AmbientLight.mat")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.AmbientLight"));
	}
	if (FileName == "DirectionalLight.mat")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.DirectionalLight"));
	}
	if (FileName == "PointLight.mat")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.PointLight"));
	}
	if (FileName == "SpotLight.mat")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.SpotLight"));
	}
	if (FileName == "HeightFog.mat")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.HeightFog"));
	}
	if (FileName == "Decal.mat")
	{
		return FResourceManager::Get().ResolvePath(FName("Editor.Billboard.Decal"));
	}

	return InPath;
}
}

FPrimitiveSceneProxy* UBillboardComponent::CreateSceneProxy()
{
	return new FBillboardSceneProxy(this);
}

void UBillboardComponent::Serialize(FArchive& Ar)
{
	UPrimitiveComponent::Serialize(Ar);
	Ar << bIsBillboard;
	Ar << TextureSlot.Path;
	Ar << Width;
	Ar << Height;
}

void UBillboardComponent::PostDuplicate()
{
	UPrimitiveComponent::PostDuplicate();

	if (!TextureSlot.Path.empty() && TextureSlot.Path != "None")
	{
		ResolveTextureFromPath(TextureSlot.Path);
	}
}

void UBillboardComponent::SetTexture(UTexture2D* InTexture)
{
	Texture = InTexture;
	if (Texture)
	{
		TextureSlot.Path = Texture->GetSourcePath();
	}
	else
	{
		TextureSlot.Path = "None";
	}
	// ?띿뒪泥?蹂寃????뚮뜑 ?ㅽ뀒?댄듃? ?꾨줉??媛깆떊
	MarkProxyDirty(EDirtyFlag::Material);
	MarkProxyDirty(EDirtyFlag::Mesh);
}

void UBillboardComponent::SetMaterial(UMaterial* InMaterial)
{
	UTexture2D* PreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(InMaterial);
	if (PreviewTexture)
	{
		SetTexture(PreviewTexture);
	}
	else
	{
		SetTexture(nullptr);
		if (InMaterial)
		{
			TextureSlot.Path = InMaterial->GetAssetPathFileName();
		}
	}
}

void UBillboardComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Texture", EPropertyType::TextureSlot, &TextureSlot });
	OutProps.push_back({ "Width",  EPropertyType::Float, &Width,  0.1f, 100.0f, 0.1f });
	OutProps.push_back({ "Height", EPropertyType::Float, &Height, 0.1f, 100.0f, 0.1f });
}

void UBillboardComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Texture") == 0)
	{
		if (TextureSlot.Path == "None" || TextureSlot.Path.empty())
		{
			SetTexture(nullptr);
		}
		else
		{
			ResolveTextureFromPath(TextureSlot.Path);
		}
		MarkRenderStateDirty();
	}
	else if (strcmp(PropertyName, "Width") == 0 || strcmp(PropertyName, "Height") == 0)
	{
		// Width/Height??鍮뚮낫??荑쇰뱶 ?ш린瑜?寃곗젙?섎?濡??몃옖?ㅽ뤌/?붾뱶 諛붿슫??紐⑤몢 媛깆떊?댁빞 ?쒕떎.
		MarkProxyDirty(EDirtyFlag::Transform);
		MarkWorldBoundsDirty();
	}
}

void UBillboardComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	if (!GetOwner() || !GetOwner()->GetWorld()) return;

	const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();
	if (!ActiveCamera) return;

	FVector WorldLocation = GetWorldLocation();
	FVector CameraForward = ActiveCamera->GetForwardVector().Normalized();
	FVector Forward = CameraForward * -1;
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f); // ?꾩떆 Up異?蹂寃?
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

	UpdateWorldAABB();
}

FMatrix UBillboardComponent::ComputeBillboardMatrix(const FVector& CameraForward) const
{
	// TickComponent? ?숈씪??濡쒖쭅
	FVector Forward = (CameraForward * -1.0f).Normalized();
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f);
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	return FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(GetWorldLocation());
}

bool UBillboardComponent::ResolveTextureFromPath(const FString& InPath)
{
	if (InPath.empty() || InPath == "None")
	{
		SetTexture(nullptr);
		return true;
	}

	const FString ResolvedPath = ResolveLegacyBillboardTexturePath(InPath);
	const std::filesystem::path Path(FPaths::ToWide(ResolvedPath));
	std::wstring Ext = Path.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), towlower);

	if (Ext == L".mat")
	{
		UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(ResolvedPath);
		if (!LoadedMat)
		{
			return false;
		}

		UTexture2D* PreviewTexture = FMaterialManager::Get().GetMaterialPreviewTexture(LoadedMat);
		if (!PreviewTexture)
		{
			SetTexture(nullptr);
			TextureSlot.Path = ResolvedPath;
			return false;
		}

		SetTexture(PreviewTexture);
		return true;
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	UTexture2D* LoadedTexture = UTexture2D::LoadFromFile(ResolvedPath, Device);
	if (!LoadedTexture)
	{
		return false;
	}

	SetTexture(LoadedTexture);
	return true;
}
