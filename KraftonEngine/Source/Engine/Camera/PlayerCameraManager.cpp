#include "PlayerCameraManager.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetCurveUtils.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Camera/CameraShakePattern.h"
#include "Engine/Core/Log.h"
#include "Engine/GameFramework/PlayerController.h"
#include "Engine/GameFramework/PawnActor.h"
#include "Object/ObjectFactory.h"

#include <algorithm>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

namespace
{
	UCameraModifier* BuildModifierFromAssetDesc(const FCameraShakeModifierAssetDesc& Desc)
	{
		UCameraModifier* Modifier = UObjectManager::Get().CreateObject<UCameraModifier>();
		if (!Modifier)
		{
			return nullptr;
		}

		UCameraShakePattern* Pattern = UObjectManager::Get().CreateObject<UCameraShakePattern>();
		if (!Pattern)
		{
			UObjectManager::Get().DestroyObject(Modifier);
			return nullptr;
		}

		Pattern->SetTransitionCurveX(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationX, Desc.LocationAmplitude.X * Desc.Intensity));
		Pattern->SetTransitionCurveY(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationY, Desc.LocationAmplitude.Y * Desc.Intensity));
		Pattern->SetTransitionCurveZ(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationZ, Desc.LocationAmplitude.Z * Desc.Intensity));
		Pattern->SetRotationCurveX(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationX, Desc.RotationAmplitude.Roll * Desc.Intensity));
		Pattern->SetRotationCurveY(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationY, Desc.RotationAmplitude.Pitch * Desc.Intensity));
		Pattern->SetRotationCurveZ(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationZ, Desc.RotationAmplitude.Yaw * Desc.Intensity));

		Modifier->Priority = Desc.Common.Priority;
		Modifier->SetAlphaInTime(Desc.Common.AlphaInTime);
		Modifier->SetAlphaOutTime(Desc.Common.AlphaOutTime);
		Modifier->SetCameraShakePattern(Pattern);
		return Modifier;
	}
}

void FViewTarget::SetNewTarget(AActor* NewTarget)
{
	if (!NewTarget) return;
	Target = NewTarget;
}

APawnActor* FViewTarget::GetTargetPawn() const
{
	if (!Target) return nullptr;
	return Cast<APawnActor>(Target);
}

bool FViewTarget::Equal(const FViewTarget& OtherTarget) const
{
	if (!POV || !OtherTarget.POV)
	{
		return POV == OtherTarget.POV;
	}
	return false;
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{

}

void APlayerCameraManager::BeginPlay()
{
	AActor::BeginPlay();
}

void APlayerCameraManager::EndPlay()
{
	ModifierList.clear();
	AActor::EndPlay();
}

void APlayerCameraManager::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	ApplyCameraModifiers(DeltaTime, ViewTarget.POV);
}

void APlayerCameraManager::LoadCameraModifierStackAsset(const std::filesystem::path& AssetPath)
{
	FString Error;
	UAssetData* LoadedAsset = FAssetFileSerializer::LoadAssetFromFile(AssetPath, &Error);
	if (!LoadedAsset)
	{
		UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to load camera modifier asset: %s", Error.c_str());
		return;
	}

	UCameraModifierStackAssetData* StackAsset = Cast<UCameraModifierStackAssetData>(LoadedAsset);
	if (!StackAsset)
	{
		UE_LOG_CATEGORY(PlayerCameraManager, Error, "Asset type mismatch: %s", AssetPath.string().c_str());
		UObjectManager::Get().DestroyObject(LoadedAsset);
		return;
	}

	for (const FCameraShakeModifierAssetDesc& Desc : StackAsset->CameraShakes)
	{
		UCameraModifier* Modifier = BuildModifierFromAssetDesc(Desc);
		if (!Modifier)
		{
			UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to create camera modifier from asset entry: %s", Desc.Name.c_str());
			continue;
		}

		AddCameraModifier(Modifier);
		if (Desc.Common.bStartDisabled)
		{
			Modifier->DisableModifier(true);
		}
	}

	UObjectManager::Get().DestroyObject(LoadedAsset);
}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* InModifier)
{
	if (!InModifier) return;

	for (UCameraModifier* ExistingModifier : ModifierList)
	{
		if (ExistingModifier == InModifier)
		{
			InModifier->EnableModifier();
			return;
		}
	}

	InModifier->AddedToCamera(this);
	InModifier->EnableModifier();
	ModifierList.push_back(InModifier);
	std::sort(ModifierList.begin(), ModifierList.end(), [](const UCameraModifier* A, const UCameraModifier* B) {
		return A->Priority > B->Priority;
	});
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, UCameraComponent* InOutPOV)
{
	if (!InOutPOV)
	{
		return;
	}

	for (UCameraModifier* CameraModifier : ModifierList)
	{
		if (!CameraModifier || CameraModifier->IsDisabled())
		{
			continue;
		}

		CameraModifier->UpdateAlpha(DeltaTime);
		if (CameraModifier->IsDisabled())
		{
			continue;
		}

		const bool bStopProcessing = CameraModifier->ModifyCamera(DeltaTime, InOutPOV);
		if (bStopProcessing)
		{
			break;
		}
	}
}
