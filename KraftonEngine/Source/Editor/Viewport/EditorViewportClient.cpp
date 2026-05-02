#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputTrigger.h"
#include "Engine/Profiling/PlatformTime.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"
#include "GameFramework/World.h"
#include "Engine/Runtime/Engine.h"
#include "Math/Vector.h"
#include "Math/MathUtils.h"

#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Collision/RayUtils.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "Viewport/GameViewportClient.h"
#include "ImGui/imgui.h"
#include "Component/Light/LightComponentBase.h"

UWorld* FEditorViewportClient::GetWorld() const
{
	return GEngine ? GEngine->GetWorld() : nullptr;
}

FEditorViewportClient::FEditorViewportClient()
{
	SetupInput();
}

FEditorViewportClient::~FEditorViewportClient()
{
	EnhancedInputManager.ClearBindings();
	EnhancedInputManager.ClearAllMappingContexts();

	if (EditorMappingContext)
	{
		for (auto& Mapping : EditorMappingContext->Mappings)
		{
			for (auto* Trigger : Mapping.Triggers) delete Trigger;
			for (auto* Modifier : Mapping.Modifiers) delete Modifier;
		}
		delete EditorMappingContext;
	}

	delete ActionEditorMove;
	delete ActionEditorRotate;
	delete ActionEditorPan;
	delete ActionEditorZoom;
	delete ActionEditorOrbit;
	delete ActionEditorFocus;
	delete ActionEditorDelete;
	delete ActionEditorDuplicate;
	delete ActionEditorToggleGizmoMode;
	delete ActionEditorToggleCoordSystem;
	delete ActionEditorEscape;
	delete ActionEditorTogglePIE;
}

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
}

void FEditorViewportClient::SetupInput()
{
	// Create Actions
	ActionEditorMove = new FInputAction("IA_EditorMove", EInputActionValueType::Axis3D);
	ActionEditorRotate = new FInputAction("IA_EditorRotate", EInputActionValueType::Axis2D);
	ActionEditorPan = new FInputAction("IA_EditorPan", EInputActionValueType::Axis2D);
	ActionEditorZoom = new FInputAction("IA_EditorZoom", EInputActionValueType::Float);
	ActionEditorOrbit = new FInputAction("IA_EditorOrbit", EInputActionValueType::Axis2D);

	ActionEditorFocus = new FInputAction("IA_EditorFocus", EInputActionValueType::Bool);
	ActionEditorDelete = new FInputAction("IA_EditorDelete", EInputActionValueType::Bool);
	ActionEditorDuplicate = new FInputAction("IA_EditorDuplicate", EInputActionValueType::Bool);
	ActionEditorToggleGizmoMode = new FInputAction("IA_EditorToggleGizmoMode", EInputActionValueType::Bool);
	ActionEditorToggleCoordSystem = new FInputAction("IA_EditorToggleCoordSystem", EInputActionValueType::Bool);
	ActionEditorEscape = new FInputAction("IA_EditorEscape", EInputActionValueType::Bool);
	ActionEditorTogglePIE = new FInputAction("IA_EditorTogglePIE", EInputActionValueType::Bool);

	// Create Mapping Context
	EditorMappingContext = new FInputMappingContext();
	EditorMappingContext->ContextName = "IMC_Editor";

	// Navigation 
	// Move: WASD QE
	EditorMappingContext->AddMapping(ActionEditorMove, 'W');
	EditorMappingContext->AddMapping(ActionEditorMove, 'S').Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
	EditorMappingContext->AddMapping(ActionEditorMove, 'D').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->AddMapping(ActionEditorMove, 'A').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
	EditorMappingContext->AddMapping(ActionEditorMove, 'E').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	EditorMappingContext->AddMapping(ActionEditorMove, 'Q').Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, 1, -1)));

	// Rotate: Arrows + Right Mouse
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_UP).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_DOWN).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	EditorMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_LEFT).Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
	EditorMappingContext->AddMapping(ActionEditorRotate, VK_RIGHT);
	
	// Mouse Rotate 
	EditorMappingContext->AddMapping(ActionEditorRotate, static_cast<int32>(EInputKey::MouseX));
	EditorMappingContext->AddMapping(ActionEditorRotate, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// Pan: Middle Mouse
	EditorMappingContext->AddMapping(ActionEditorPan, static_cast<int32>(EInputKey::MouseX));
	EditorMappingContext->AddMapping(ActionEditorPan, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// Zoom: Wheel
	EditorMappingContext->AddMapping(ActionEditorZoom, static_cast<int32>(EInputKey::MouseWheel));

	// Orbit: Alt + Left Mouse
	EditorMappingContext->AddMapping(ActionEditorOrbit, static_cast<int32>(EInputKey::MouseX));
	EditorMappingContext->AddMapping(ActionEditorOrbit, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	//Shortcuts 
	EditorMappingContext->AddMapping(ActionEditorFocus, 'F');
	EditorMappingContext->AddMapping(ActionEditorDelete, VK_DELETE);
	EditorMappingContext->AddMapping(ActionEditorDuplicate, 'D');
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, VK_SPACE);
	EditorMappingContext->AddMapping(ActionEditorToggleCoordSystem, 'X');
	EditorMappingContext->AddMapping(ActionEditorEscape, VK_ESCAPE);
	EditorMappingContext->AddMapping(ActionEditorTogglePIE, VK_F8);

	// Gizmo Shortcuts (QWER)
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'W'); // Translate
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'E'); // Rotate
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'R'); // Scale
	EditorMappingContext->AddMapping(ActionEditorToggleGizmoMode, 'Q'); // Select

	// Game View Toggle (G)
	FInputAction* ActionEditorToggleGameView = new FInputAction("IA_EditorToggleGameView", EInputActionValueType::Bool);
	EditorMappingContext->AddMapping(ActionEditorToggleGameView, 'G');
	EnhancedInputManager.BindAction(ActionEditorToggleGameView, ETriggerEvent::Started, [this](const FInputActionValue& V) {
		RenderOptions.bGameView = !RenderOptions.bGameView;
	});

	// Add Context
	EnhancedInputManager.AddMappingContext(EditorMappingContext, 0);

	// Bind Actions
	EnhancedInputManager.BindAction(ActionEditorMove, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorMove(V); });
	EnhancedInputManager.BindAction(ActionEditorRotate, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorRotate(V); });
	EnhancedInputManager.BindAction(ActionEditorPan, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorPan(V); });
	EnhancedInputManager.BindAction(ActionEditorZoom, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorZoom(V); });
	EnhancedInputManager.BindAction(ActionEditorOrbit, ETriggerEvent::Triggered, [this](const FInputActionValue& V) { OnEditorOrbit(V); });

	EnhancedInputManager.BindAction(ActionEditorFocus, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorFocus(V); });
	EnhancedInputManager.BindAction(ActionEditorDelete, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorDelete(V); });
	EnhancedInputManager.BindAction(ActionEditorDuplicate, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorDuplicate(V); });
	EnhancedInputManager.BindAction(ActionEditorToggleGizmoMode, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorToggleGizmoMode(V); });
	EnhancedInputManager.BindAction(ActionEditorToggleCoordSystem, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorToggleCoordSystem(V); });
	EnhancedInputManager.BindAction(ActionEditorEscape, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorEscape(V); });
	EnhancedInputManager.BindAction(ActionEditorTogglePIE, ETriggerEvent::Started, [this](const FInputActionValue& V) { OnEditorTogglePIE(V); });
}

void FEditorViewportClient::OnEditorMove(const FInputActionValue& Value)
{
	if (FInputManager::Get().IsKeyDown(VK_CONTROL)) return;
	EditorMoveAccumulator = EditorMoveAccumulator + Value.GetVector();
}

void FEditorViewportClient::OnEditorRotate(const FInputActionValue& Value)
{
	bool bIsKeyboard = std::abs(Value.GetVector().X) > 0.1f && !FInputManager::Get().IsMouseButtonDown(VK_RBUTTON);
	if (FInputManager::Get().IsMouseButtonDown(VK_RBUTTON) || bIsKeyboard)
	{
		EditorRotateAccumulator = EditorRotateAccumulator + Value.GetVector();
	}
}

void FEditorViewportClient::OnEditorPan(const FInputActionValue& Value)
{
	if (FInputManager::Get().IsMouseButtonDown(VK_MBUTTON))
	{
		EditorPanAccumulator = EditorPanAccumulator + Value.GetVector();
	}
}

void FEditorViewportClient::OnEditorZoom(const FInputActionValue& Value)
{
	FInputManager& Input = FInputManager::Get();
	//  Hold RMB + Scroll = Change Camera Speed
	if (Input.IsMouseButtonDown(VK_RBUTTON))
	{
		float& Speed = FEditorSettings::Get().CameraSpeed;
		Speed = Clamp(Speed + Value.Get() * 2.0f, 1.0f, 100.0f);
		return;
	}

	EditorZoomAccumulator += Value.Get();
}

void FEditorViewportClient::OnEditorOrbit(const FInputActionValue& Value)
{
	if (FInputManager::Get().IsKeyDown(VK_MENU) && FInputManager::Get().IsMouseButtonDown(VK_LBUTTON))
	{
		EditorRotateAccumulator = EditorRotateAccumulator + Value.GetVector();
	}
}

void FEditorViewportClient::OnEditorFocus(const FInputActionValue& Value)
{
	if (SelectionManager && Camera)
	{
		AActor* Selected = SelectionManager->GetPrimarySelection();
		if (Selected)
		{
			FVector TargetLoc = Selected->GetActorLocation();
			FVector CameraForward = Camera->GetForwardVector();
			FVector OriginalLoc = Camera->GetWorldLocation();
			FRotator OriginalRot = Camera->GetRelativeRotation();
			float FocusDistance = 5.0f;
			FVector NewCameraLoc = TargetLoc - CameraForward * FocusDistance;
			Camera->SetWorldLocation(NewCameraLoc);
			Camera->LookAt(TargetLoc);
			FRotator TargetRot = Camera->GetRelativeRotation();
			Camera->SetWorldLocation(OriginalLoc);
			Camera->SetRelativeRotation(OriginalRot);
			bIsFocusAnimating = true;
			FocusAnimTimer = 0.0f;
			FocusStartLoc = OriginalLoc;
			FocusStartRot = OriginalRot;
			FocusEndLoc = NewCameraLoc;
			FocusEndRot = TargetRot;
		}
	}
}

void FEditorViewportClient::OnEditorDelete(const FInputActionValue& Value)
{
	if (SelectionManager) SelectionManager->DeleteSelectedActors();
}

void FEditorViewportClient::OnEditorDuplicate(const FInputActionValue& Value)
{
	if (SelectionManager && FInputManager::Get().IsKeyDown(VK_CONTROL))
	{
		const TArray<AActor*> ToDuplicate = SelectionManager->GetSelectedActors();
		if (!ToDuplicate.empty())
		{
			const FVector DuplicateOffsetStep(0.1f, 0.1f, 0.1f);
			TArray<AActor*> NewSelection;
			int32 DuplicateIndex = 0;
			for (AActor* Src : ToDuplicate)
			{
				if (!Src) continue;
				AActor* Dup = Cast<AActor>(Src->Duplicate(nullptr));
				if (Dup)
				{
					Dup->AddActorWorldOffset(DuplicateOffsetStep * static_cast<float>(DuplicateIndex + 1));
					NewSelection.push_back(Dup);
					++DuplicateIndex;
				}
			}
			SelectionManager->ClearSelection();
			for (AActor* Actor : NewSelection) SelectionManager->ToggleSelect(Actor);
			if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			{
				if (EditorEngine->GetGizmo()) EditorEngine->GetGizmo()->UpdateGizmoTransform();
			}
		}
	}
}
void FEditorViewportClient::OnEditorToggleGizmoMode(const FInputActionValue& Value)
{
	if (Gizmo)
	{
		FInputManager& Input = FInputManager::Get();
		if (Input.IsKeyPressed('W')) Gizmo->UpdateGizmoMode(EGizmoMode::Translate);
		else if (Input.IsKeyPressed('E')) Gizmo->UpdateGizmoMode(EGizmoMode::Rotate);
		else if (Input.IsKeyPressed('R')) Gizmo->UpdateGizmoMode(EGizmoMode::Scale);
		else if (Input.IsKeyPressed('Q')) Gizmo->UpdateGizmoMode(EGizmoMode::Select);
		else Gizmo->SetNextMode(); 

		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			EditorEngine->ApplyTransformSettingsToGizmo();
	}
}


void FEditorViewportClient::OnEditorToggleCoordSystem(const FInputActionValue& Value)
{
	if (!FInputManager::Get().IsKeyDown(VK_CONTROL))
	{
		if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
			EditorEngine->ToggleCoordSystem();
	}
}

void FEditorViewportClient::OnEditorEscape(const FInputActionValue& Value)
{
	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor()) EditorEngine->RequestEndPlayMap();
	}
}

void FEditorViewportClient::OnEditorTogglePIE(const FInputActionValue& Value)
{
	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor()) EditorEngine->TogglePIEControlMode();
	}
}

void FEditorViewportClient::SetLightViewOverride(ULightComponentBase* Light)
{
	LightViewOverride = Light;
	PointLightFaceIndex = 0;

	if (Light && SelectionManager)
	{
		SelectionManager->ClearSelection();
	}
}

void FEditorViewportClient::ClearLightViewOverride()
{
	LightViewOverride = nullptr;
}

void FEditorViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
}

void FEditorViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}
}

void FEditorViewportClient::ResetCamera()
{
	if (!Camera || !Settings) return;
	Camera->SetWorldLocation(Settings->InitViewPos);
	Camera->LookAt(Settings->InitLookAt);
	SyncCameraSmoothingTarget();
}

void FEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
	if (!Camera) return;

	RenderOptions.ViewportType = NewType;

	if (NewType == ELevelViewportType::Perspective)
	{
		Camera->SetOrthographic(false);
		SyncCameraSmoothingTarget();
		return;
	}

	if (NewType == ELevelViewportType::FreeOrthographic)
	{
		Camera->SetOrthographic(true);
		SyncCameraSmoothingTarget();
		return;
	}

	// 고정 방향 Orthographic: 카메라를 프리셋 방향으로 설정
	Camera->SetOrthographic(true);
	constexpr float OrthoDistance = 50.0f;
	FVector Position = FVector(0, 0, 0);
	FVector Rotation = FVector(0, 0, 0); // (Roll, Pitch, Yaw)

	switch (NewType)
	{
	case ELevelViewportType::Top:
		Position = FVector(0, 0, OrthoDistance);
		Rotation = FVector(0, 90.0f, 0);	// Pitch down (positive pitch = look -Z)
		break;
	case ELevelViewportType::Bottom:
		Position = FVector(0, 0, -OrthoDistance);
		Rotation = FVector(0, -90.0f, 0);	// Pitch up (negative pitch = look +Z)
		break;
	case ELevelViewportType::Front:
		Position = FVector(OrthoDistance, 0, 0);
		Rotation = FVector(0, 0, 180.0f);	// Yaw to look -X
		break;
	case ELevelViewportType::Back:
		Position = FVector(-OrthoDistance, 0, 0);
		Rotation = FVector(0, 0, 0.0f);		// Yaw to look +X
		break;
	case ELevelViewportType::Left:
		Position = FVector(0, -OrthoDistance, 0);
		Rotation = FVector(0, 0, 90.0f);	// Yaw to look +Y
		break;
	case ELevelViewportType::Right:
		Position = FVector(0, OrthoDistance, 0);
		Rotation = FVector(0, 0, -90.0f);	// Yaw to look -Y
		break;
	default:
		break;
	}

	Camera->SetRelativeLocation(Position);
	Camera->SetRelativeRotation(Rotation);
	SyncCameraSmoothingTarget();
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	if (InWidth > 0.0f)
	{
		WindowWidth = InWidth;
	}

	if (InHeight > 0.0f)
	{
		WindowHeight = InHeight;
	}

	if (Camera)
	{
		Camera->OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
	}
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (!bIsActive) return;

	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		if (EditorEngine->IsPlayingInEditor())
		{
			FInputManager& Input = FInputManager::Get();
			if (Input.IsKeyPressed(VK_ESCAPE)) { EditorEngine->RequestEndPlayMap(); return; }
			if (Input.IsKeyPressed(VK_F8)) { EditorEngine->TogglePIEControlMode(); }

			if (EditorEngine->IsPIEPossessedMode())
			{
				if (UGameViewportClient* GameViewportClient = EditorEngine->GetGameViewportClient())
				{
					GameViewportClient->SetDrivingCamera(Camera);
					GameViewportClient->SetViewport(Viewport);
					GameViewportClient->ProcessPIEInput(DeltaTime);
				}
				return;
			}
		}
	}

	SyncCameraSmoothingTarget();

	// Camera Focus Animation Update
	if (bIsFocusAnimating && Camera)
	{
		FocusAnimTimer += DeltaTime;
		float Alpha = FocusAnimTimer / FocusAnimDuration;
		if (Alpha >= 1.0f)
		{
			Alpha = 1.0f;
			bIsFocusAnimating = false;
		}

		// SmoothStep curve for better feel
		float SmoothAlpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
		FVector NewLoc = FocusStartLoc * (1.0f - SmoothAlpha) + FocusEndLoc * SmoothAlpha;
		
		// Rotation Interpolation (Slerp-like for Rotators)
		FQuat StartQuat = FocusStartRot.ToQuaternion();
		FQuat EndQuat = FocusEndRot.ToQuaternion();
		FQuat BlendedQuat = FQuat::Slerp(StartQuat, EndQuat, SmoothAlpha);
		Camera->SetWorldLocation(NewLoc);
		Camera->SetRelativeRotation(FRotator::FromQuaternion(BlendedQuat));

		// Sync TargetLocation during animation to prevent jumping after focus ends
		TargetLocation = NewLoc;
		LastAppliedCameraLocation = NewLoc;
		bLastAppliedCameraLocationInitialized = true;
	}
	else
	{
		ApplySmoothedCameraLocation(DeltaTime);
	}

	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
}

void FEditorViewportClient::SyncCameraSmoothingTarget()
{
	if (!Camera)
	{
		bTargetLocationInitialized = false;
		bLastAppliedCameraLocationInitialized = false;
		return;
	}

	const FVector CurrentLocation = Camera->GetWorldLocation();
	const bool bCameraMovedExternally =
		bLastAppliedCameraLocationInitialized &&
		FVector::DistSquared(CurrentLocation, LastAppliedCameraLocation) > 0.0001f;

	if (!bTargetLocationInitialized || bCameraMovedExternally)
	{
		TargetLocation = CurrentLocation;
		bTargetLocationInitialized = true;
	}

	LastAppliedCameraLocation = CurrentLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FEditorViewportClient::ApplySmoothedCameraLocation(float DeltaTime)
{
	if (!Camera) return;
	const FVector CurrentLocation = Camera->GetWorldLocation();
	const float LerpAlpha = Clamp(DeltaTime * SmoothLocationSpeed, 0.0f, 1.0f);
	const FVector NewLocation = CurrentLocation + (TargetLocation - CurrentLocation) * LerpAlpha;
	Camera->SetWorldLocation(NewLocation);
	LastAppliedCameraLocation = NewLocation;
	bLastAppliedCameraLocationInitialized = true;
}

void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!Camera) return;
	if (IsViewingFromLight()) return;

	FInputManager& Input = FInputManager::Get();
	if (Input.IsGuiUsingKeyboard()) return;

	EditorMoveAccumulator = FVector::ZeroVector;
	EditorRotateAccumulator = FVector::ZeroVector;
	EditorPanAccumulator = FVector::ZeroVector;
	EditorZoomAccumulator = 0.0f;

	EnhancedInputManager.ProcessInput(&Input, DeltaTime);

	const FCameraState& CameraState = Camera->GetCameraState();
	const bool bIsOrtho = CameraState.bIsOrthogonal;
	const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
	const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;
	const float PanMouseScale = CameraSpeed * 0.01f;

	if (!bIsOrtho)
	{
		FVector DeltaMove = (Camera->GetForwardVector() * EditorMoveAccumulator.X + Camera->GetRightVector() * EditorMoveAccumulator.Y) * (CameraSpeed * DeltaTime);
		DeltaMove.Z += EditorMoveAccumulator.Z * (CameraSpeed * DeltaTime);
		TargetLocation += DeltaMove;

		if (!EditorPanAccumulator.IsNearlyZero())
		{
			FVector PanDelta = (Camera->GetRightVector() * (-EditorPanAccumulator.X * PanMouseScale * 0.15f)) + (Camera->GetUpVector() * (EditorPanAccumulator.Y * PanMouseScale * 0.15f));
			TargetLocation += PanDelta;
		}

		if (!EditorRotateAccumulator.IsNearlyZero())
		{
			const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
			const float MouseRotationSpeed = 0.15f * RotateSensitivity;
			const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
			float Yaw = 0.0f, Pitch = 0.0f;
			if (std::abs(EditorRotateAccumulator.Y) > 1.1f || std::abs(EditorRotateAccumulator.Z) > 1.1f) { Yaw = EditorRotateAccumulator.Y * AngleVelocity * DeltaTime; Pitch = EditorRotateAccumulator.Z * AngleVelocity * DeltaTime; }
			else { Yaw = EditorRotateAccumulator.Y * MouseRotationSpeed; Pitch = EditorRotateAccumulator.Z * MouseRotationSpeed; }
			Camera->Rotate(Yaw, Pitch);
		}
	}
	else
	{
		if (!EditorRotateAccumulator.IsNearlyZero() && Input.IsMouseButtonDown(VK_RBUTTON))
		{
			float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
			Camera->MoveLocal(FVector(0, -EditorRotateAccumulator.Y * PanScale, EditorRotateAccumulator.Z * PanScale));
		}
	}
}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;
	if (!Camera || !Gizmo || !GetWorld()) return;

	Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(), Camera->IsOrthogonal(), Camera->GetOrthoWidth());
	Gizmo->SetAxisMask(UGizmoComponent::ComputeAxisMask(RenderOptions.ViewportType, Gizmo->GetMode()));

	const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;

	if (std::abs(EditorZoomAccumulator) > 1e-6f)
	{
		if (Camera->IsOrthogonal()) { float NewWidth = Camera->GetOrthoWidth() - EditorZoomAccumulator * ZoomSpeed * DeltaTime; Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f)); }
		else { TargetLocation += Camera->GetForwardVector() * (EditorZoomAccumulator * ZoomSpeed * 0.015f); }
	}

	FInputManager& Input = FInputManager::Get();
	ImVec2 MousePos = ImGui::GetIO().MousePos;
	float LocalMouseX = MousePos.x - ViewportScreenRect.X;
	float LocalMouseY = MousePos.y - ViewportScreenRect.Y;
	float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
	float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
	FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	FRayHitResult HitResult;
	FRayUtils::RaycastComponent(Gizmo, Ray, HitResult);

	if (Input.IsKeyPressed(FInputManager::MOUSE_LEFT))
	{
		if (Input.IsKeyDown(VK_CONTROL) && Input.IsKeyDown(VK_MENU)) { bIsMarqueeSelecting = true; MarqueeStartPos = FVector(MousePos.x, MousePos.y, 0); MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0); }
		else { HandleDragStart(Ray); }
	}
	else if (Input.IsMouseButtonDown(FInputManager::MOUSE_LEFT))
	{
		if (bIsMarqueeSelecting) { MarqueeCurrentPos = FVector(MousePos.x, MousePos.y, 0); }
		else { if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding()) Gizmo->SetHolding(true); if (Gizmo->IsHolding()) Gizmo->UpdateDrag(Ray); }
	}
	else if (Input.IsKeyReleased(FInputManager::MOUSE_LEFT))
	{
		if (bIsMarqueeSelecting)
		{
			bIsMarqueeSelecting = false;
			float MinX = (std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X);
			float MaxX = (std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X);
			float MinY = (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);
			float MaxY = (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y);
			if (std::abs(MaxX - MinX) > 2.0f || std::abs(MaxY - MinY) > 2.0f)
			{
				UWorld* World = GetWorld();
				if (World && SelectionManager)
				{
					if (!Input.IsKeyDown(VK_CONTROL)) SelectionManager->ClearSelection();
					FMatrix VP = Camera->GetViewProjectionMatrix();
					for (AActor* Actor : World->GetActors())
					{
						if (!Actor || !Actor->IsVisible() || Actor->IsA<UGizmoComponent>()) continue;
						FVector WorldPos = Actor->GetActorLocation();
						FVector ClipSpace = VP.TransformPositionWithW(WorldPos);
						float ScreenX = (ClipSpace.X * 0.5f + 0.5f) * VPWidth + ViewportScreenRect.X;
						float ScreenY = (1.0f - (ClipSpace.Y * 0.5f + 0.5f)) * VPHeight + ViewportScreenRect.Y;
						if (ScreenX >= MinX && ScreenX <= MaxX && ScreenY >= MinY && ScreenY <= MaxY) SelectionManager->ToggleSelect(Actor);
					}
				}
			}
		}
		else { Gizmo->DragEnd(); }
	}
}

void FEditorViewportClient::HandleDragStart(const FRay& Ray)
{
	FScopeCycleCounter PickCounter;
	FRayHitResult HitResult{};
	if (FRayUtils::RaycastComponent(Gizmo, Ray, HitResult)) { Gizmo->SetPressedOnHandle(true); }
	else
	{
		AActor* BestActor = nullptr;
		if (UWorld* W = GetWorld()) { W->RaycastPrimitives(Ray, HitResult, BestActor); }
		bool bCtrlHeld = FInputManager::Get().IsKeyDown(VK_CONTROL);
		if (BestActor == nullptr) { if (!bCtrlHeld) SelectionManager->ClearSelection(); }
		else
		{
			if (bCtrlHeld) SelectionManager->ToggleSelect(BestActor);
			else { if (SelectionManager->GetPrimarySelection() == BestActor) { if (HitResult.HitComponent) SelectionManager->SelectComponent(HitResult.HitComponent); } else SelectionManager->Select(BestActor); }
		}
	}
	if (OverlayStatSystem) { const uint64 PickCycles = PickCounter.Finish(); const double ElapsedMs = FPlatformTime::ToMilliseconds(PickCycles); OverlayStatSystem->RecordPickingAttempt(ElapsedMs); }
}

void FEditorViewportClient::UpdateLayoutRect()
{
	if (!LayoutWindow) return;
	const FRect& R = LayoutWindow->GetRect();
	ViewportScreenRect = R;
	if (Viewport) { uint32 SlotW = static_cast<uint32>(R.Width); uint32 SlotH = static_cast<uint32>(R.Height); if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight())) Viewport->RequestResize(SlotW, SlotH); }
}

void FEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
	if (!Viewport || !Viewport->GetSRV()) return;
	const FRect& R = ViewportScreenRect;
	if (R.Width <= 0 || R.Height <= 0) return;
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 Min(R.X, R.Y);
	ImVec2 Max(R.X + R.Width, R.Y + R.Height);
	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);
	if (bIsActiveViewport) DrawList->AddRect(Min, Max, IM_COL32(255, 165, 0, 220), 0.0f, 0, 2.0f);
	if (bIsMarqueeSelecting)
	{
		ImDrawList* ForegroundDrawList = ImGui::GetForegroundDrawList();
		ImVec2 RectMin((std::min)(MarqueeStartPos.X, MarqueeCurrentPos.X), (std::min)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
		ImVec2 RectMax((std::max)(MarqueeStartPos.X, MarqueeCurrentPos.X), (std::max)(MarqueeStartPos.Y, MarqueeCurrentPos.Y));
		ForegroundDrawList->AddRect(RectMin, RectMax, IM_COL32(255, 255, 255, 255), 0.0f, 0, 5.0f);
	}
}

bool FEditorViewportClient::GetCursorViewportPosition(uint32& OutX, uint32& OutY) const
{
	if (!bIsActive) return false;
	ImVec2 MousePos = ImGui::GetIO().MousePos;
	float LocalX = MousePos.x - ViewportScreenRect.X;
	float LocalY = MousePos.y - ViewportScreenRect.Y;
	if (LocalX >= 0 && LocalY >= 0 && LocalX < ViewportScreenRect.Width && LocalY < ViewportScreenRect.Height) { OutX = static_cast<uint32>(LocalX); OutY = static_cast<uint32>(LocalY); return true; }
	return false;
}

void FEditorViewportClient::TickEditorShortcuts() {}
