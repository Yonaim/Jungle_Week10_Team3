#include "PlayerCameraManager.h"

#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShake.h"
#include "Camera/CameraShakePattern.h"
#include "Camera/LuaCameraModifier.h"
#include "Component/ActorComponent.h"
#include "Component/CameraComponent.h"
#include "Engine/Asset/AssetCurveUtils.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Core/Log.h"
#include "Engine/GameFramework/PawnActor.h"
#include "Engine/GameFramework/PlayerController.h"
#include "Object/ObjectFactory.h"

#include <algorithm>

IMPLEMENT_CLASS(APlayerCameraManager, AActor)

static UCameraShakeBase* BuildCameraShakeFromAssetDesc(UObject* Outer, const FCameraShakeModifierAssetDesc& Desc)
{
	UCameraShakeBase* Shake = UObjectManager::Get().CreateObject<UCameraShakeBase>(Outer);
	if (!Shake)
	{
		return nullptr;
	}

	UCameraShakePattern* RootPattern = nullptr;
	if (Desc.bUseCurves)
	{
		UCurveCameraShakePattern* CurvePattern = UObjectManager::Get().CreateObject<UCurveCameraShakePattern>(Shake);
		if (CurvePattern)
		{
			CurvePattern->SetTransitionCurveX(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationX, Desc.LocationAmplitude.X));
			CurvePattern->SetTransitionCurveY(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationY, Desc.LocationAmplitude.Y));
			CurvePattern->SetTransitionCurveZ(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.TranslationZ, Desc.LocationAmplitude.Z));
			CurvePattern->SetRotationCurveX(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationX, Desc.RotationAmplitude.Pitch));
			CurvePattern->SetRotationCurveY(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationY, Desc.RotationAmplitude.Yaw));
			CurvePattern->SetRotationCurveZ(FAssetCurveUtils::MakeCurveFromBezier(Desc.Curves.RotationZ, Desc.RotationAmplitude.Roll));
			RootPattern = CurvePattern;
		}
	}
	else
	{
		USinWaveCameraShakePattern* SinPattern = UObjectManager::Get().CreateObject<USinWaveCameraShakePattern>(Shake);
		if (SinPattern)
		{
			SinPattern->Frequency = Desc.Frequency;
			SinPattern->TransitionAmplitudeX = Desc.LocationAmplitude.X;
			SinPattern->TransitionAmplitudeY = Desc.LocationAmplitude.Y;
			SinPattern->TransitionAmplitudeZ = Desc.LocationAmplitude.Z;
			SinPattern->RotationAmplitudeX = Desc.RotationAmplitude.Pitch;
			SinPattern->RotationAmplitudeY = Desc.RotationAmplitude.Yaw;
			SinPattern->RotationAmplitudeZ = Desc.RotationAmplitude.Roll;
			RootPattern = SinPattern;
		}
	}

	if (!RootPattern)
	{
		UObjectManager::Get().DestroyObject(Shake);
		return nullptr;
	}

	Shake->Duration = Desc.Duration;
	Shake->Intensity = Desc.Intensity;
	Shake->SetRootShakePattern(RootPattern);

	return Shake;
}

void FViewTarget::SetNewTarget(AActor* NewTarget)
{
	if (!NewTarget)
	{
		return;
	}

	Target = NewTarget;
}

APawnActor* FViewTarget::GetTargetPawn() const
{
	if (!Target)
	{
		return nullptr;
	}

	return Cast<APawnActor>(Target);
}

bool FViewTarget::Equal(const FViewTarget& OtherTarget) const
{
	return Target != nullptr && Target == OtherTarget.Target;
}

void FViewTarget::CheckViewTarget(APlayerController* OwningController)
{
	if (!Target && OwningController)
	{
		Target = OwningController->GetPawn();
	}
}

void APlayerCameraManager::BeginPlay()
{
	AActor::BeginPlay();
}

void APlayerCameraManager::EndPlay()
{
	for (UCameraModifier* Modifier : ModifierList)
	{
		if (Modifier)
		{
			UObjectManager::Get().DestroyObject(Modifier);
		}
	}

	ModifierList.clear();
	CameraShakeModifier = nullptr;
	AActor::EndPlay();
}

void APlayerCameraManager::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	UpdateCamera(DeltaTime);
}

void APlayerCameraManager::SetOwner(APlayerController* InPlayerController)
{
	Owner = InPlayerController;
	if (Owner && Owner->GetPawn())
	{
		ViewTarget.SetNewTarget(Owner->GetPawn());
	}
}

void APlayerCameraManager::AddCameraModifier(UCameraModifier* InModifier)
{
	if (!InModifier)
	{
		return;
	}

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
	SortModifiersByPriority();
}

void APlayerCameraManager::PlayCameraModifier(const FString& ScriptPath, const TMap<FString, float>& Params)
{
	ULuaCameraModifier* Modifier = UObjectManager::Get().CreateObject<ULuaCameraModifier>(this);
	if (!Modifier)
	{
		return;
	}

	if (!Modifier->Initialize(ScriptPath, Params))
	{
		UObjectManager::Get().DestroyObject(Modifier);
		return;
	}

	AddCameraModifier(Modifier);
}

void APlayerCameraManager::ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	RemoveFinishedModifiers();

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

	RemoveFinishedModifiers();
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	bHasValidCameraCachePOV = false;
	LastFrameCameraCache = CameraCache;

	ViewTarget.CheckViewTarget(Owner);

	FMinimalViewInfo NewPOV;
	if (ViewTarget.Target)
	{
		ViewTarget.Target->CalcCamera(DeltaTime, NewPOV);
	}
	else
	{
		NewPOV = BuildFallbackCameraView(nullptr);
	}

	ApplyCameraModifiers(DeltaTime, NewPOV);

	if (bEnableFading && FadeTime > 0.0f)
	{
		if (FadeTimeRemaining < 0.0f)
		{
			FadeTimeRemaining = 0.0f;
		}

		const float Elapsed = FadeTime - FadeTimeRemaining;
		FadeAmount = FadeAlpha.X + (FadeAlpha.Y - FadeAlpha.X) * (Elapsed / FadeTime);
		FadeTimeRemaining -= DeltaTime;
	}

	NewPOV.PostPorcessSettings.FadeColor = FadeColor;
	NewPOV.PostPorcessSettings.FadeAmount = FadeAmount;

	ViewTarget.POV = NewPOV;
	CameraCache.TimeStamp += DeltaTime;
	CameraCache.POV = NewPOV;
	bHasValidCameraCachePOV = true;
}

void APlayerCameraManager::SortModifiersByPriority()
{
	std::sort(ModifierList.begin(), ModifierList.end(),
		[](const UCameraModifier* A, const UCameraModifier* B)
		{
			if (!A)
			{
				return false;
			}
			if (!B)
			{
				return true;
			}
			return A->Priority > B->Priority;
		});
}

void APlayerCameraManager::RemoveFinishedModifiers()
{
	ModifierList.erase(
		std::remove_if(ModifierList.begin(), ModifierList.end(),
			[this](UCameraModifier* Modifier)
			{
				if (!Modifier)
				{
					return true;
				}

				if (Modifier->IsFinished())
				{
					if (Modifier == CameraShakeModifier)
					{
						CameraShakeModifier = nullptr;
					}
					UObjectManager::Get().DestroyObject(Modifier);
					return true;
				}

				return false;
			}),
		ModifierList.end());
}

void APlayerCameraManager::StartCameraShake()
{
	if (CameraShakeModifier)
	{
		CameraShakeModifier->EnableModifier();
	}
}

void APlayerCameraManager::EndCameraShake()
{
	if (CameraShakeModifier)
	{
		CameraShakeModifier->DisableModifier();
	}
}

void APlayerCameraManager::StartCameraFade(float FromAlpha, float ToAlpha, float Duration, FLinearColor Color)
{
	FadeColor = Color;
	FadeAlpha = FVector2(FromAlpha, ToAlpha);
	FadeTimeRemaining = Duration;
	FadeTime = Duration;
	bEnableFading = true;
}

void APlayerCameraManager::EndCameraFade()
{
	bEnableFading = false;
	FadeAmount = 0.0f;
	FadeTimeRemaining = 0.0f;
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
		if (Desc.Common.bStartDisabled)
		{
			continue;
		}

		UCameraModifier_CameraShake* Modifier = EnsureCameraShakeModifier();
		if (!Modifier)
		{
			UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to create camera shake modifier from asset entry: %s", Desc.Name.c_str());
			continue;
		}

		UCameraShakeBase* Shake = BuildCameraShakeFromAssetDesc(Modifier, Desc);
		if (!Shake)
		{
			UE_LOG_CATEGORY(PlayerCameraManager, Error, "Failed to create camera shake from asset entry: %s", Desc.Name.c_str());
			continue;
		}

		Modifier->Priority = Desc.Common.Priority > Modifier->Priority ? Desc.Common.Priority : Modifier->Priority;
		Modifier->SetAlphaInTime(Desc.Common.AlphaInTime);
		Modifier->SetAlphaOutTime(Desc.Common.AlphaOutTime);
		Modifier->AddCameraShake(Shake);
		SortModifiersByPriority();
	}

	UObjectManager::Get().DestroyObject(LoadedAsset);
}

UCameraComponent* APlayerCameraManager::FindCameraComponent(AActor* Target)
{
	if (!Target)
	{
		return nullptr;
	}

	return Target->GetComponentByClass<UCameraComponent>();
}

FMinimalViewInfo APlayerCameraManager::BuildFallbackCameraView(AActor* Target) const
{
	FMinimalViewInfo ViewInfo;
	if (Target)
	{
		ViewInfo.Location = Target->GetActorLocation();
		ViewInfo.Rotation = Target->GetActorRotation();
	}
	else
	{
		ViewInfo.Location = FVector::ZeroVector;
		ViewInfo.Rotation = FRotator::ZeroRotator;
	}

	return ViewInfo;
}

UCameraModifier_CameraShake* APlayerCameraManager::EnsureCameraShakeModifier()
{
	if (CameraShakeModifier)
	{
		return CameraShakeModifier;
	}

	CameraShakeModifier = UObjectManager::Get().CreateObject<UCameraModifier_CameraShake>(this);
	if (!CameraShakeModifier)
	{
		return nullptr;
	}

	AddCameraModifier(CameraShakeModifier);
	return CameraShakeModifier;
}
