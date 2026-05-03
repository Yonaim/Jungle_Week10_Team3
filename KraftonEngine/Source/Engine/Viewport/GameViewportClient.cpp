#include "Viewport/GameViewportClient.h"

#include "Component/CameraComponent.h"
#include "Engine/Input/InputManager.h"
#include "Engine/Input/InputModifier.h"
#include "Engine/Input/InputTrigger.h"
#include "Math/MathUtils.h"

#include <windows.h>

DEFINE_CLASS(UGameViewportClient, UObject)

void UGameViewportClient::OnBeginPIE(UCameraComponent* InitialTarget, FViewport* InViewport)
{
	Viewport = InViewport;
	Possess(InitialTarget);
	SetupInput();
	ResetInputState();
}

void UGameViewportClient::OnEndPIE()
{
	SetPossessed(false);
	UnPossess();
	ResetInputState();
	bHasCursorClipRect = false;
	Viewport = nullptr;

	EnhancedInputManager.ClearBindings();
	EnhancedInputManager.ClearAllMappingContexts();

	if (DefaultMappingContext)
	{
		for (auto& Mapping : DefaultMappingContext->Mappings)
		{
			for (auto* Trigger : Mapping.Triggers) delete Trigger;
			for (auto* Modifier : Mapping.Modifiers) delete Modifier;
		}
		delete DefaultMappingContext;
		DefaultMappingContext = nullptr;
	}

	delete ActionMove;
	delete ActionLook;
	delete ActionSprint;
	ActionMove = nullptr;
	ActionLook = nullptr;
	ActionSprint = nullptr;
}

bool UGameViewportClient::ProcessPIEInput(float DeltaTime)
{
	return Tick(DeltaTime);
}

void UGameViewportClient::SetPIEPossessedInputEnabled(bool bEnabled)
{
	SetPossessed(bEnabled);
}

void UGameViewportClient::SetCursorClipRect(const FRect& InViewportScreenRect)
{
	if (InViewportScreenRect.Width <= 1.0f || InViewportScreenRect.Height <= 1.0f)
	{
		bHasCursorClipRect = false;
		if (bCursorCaptured)
		{
			ApplyCursorClip();
		}
		return;
	}

	CursorClipClientRect.left = static_cast<LONG>(InViewportScreenRect.X);
	CursorClipClientRect.top = static_cast<LONG>(InViewportScreenRect.Y);
	CursorClipClientRect.right = static_cast<LONG>(InViewportScreenRect.X + InViewportScreenRect.Width);
	CursorClipClientRect.bottom = static_cast<LONG>(InViewportScreenRect.Y + InViewportScreenRect.Height);
	bHasCursorClipRect = CursorClipClientRect.right > CursorClipClientRect.left
		&& CursorClipClientRect.bottom > CursorClipClientRect.top;

	if (bCursorCaptured)
	{
		ApplyCursorClip();
	}
}

void UGameViewportClient::SetPossessed(bool bPossessed)
{
	if (bPIEPossessedInputEnabled == bPossessed)
	{
		return;
	}

	bPIEPossessedInputEnabled = bPossessed;
	SetCursorCaptured(bPossessed);
	
	FInputManager::Get().SetTrackingMouse(bPossessed);
	
	ResetInputState();
}

void UGameViewportClient::Possess(UCameraComponent* TargetCamera)
{
	if (PossessedCamera == TargetCamera)
	{
		return;
	}

	PossessedCamera = TargetCamera;
	ResetInputState();
}

void UGameViewportClient::UnPossess()
{
	PossessedCamera = nullptr;
	SetCursorCaptured(false);
	FInputManager::Get().SetTrackingMouse(false);
	ResetInputState();
}

void UGameViewportClient::ResetInputState()
{
	MoveInputAccumulator = FVector::ZeroVector;
	LookInputAccumulator = FVector::ZeroVector;
}

bool UGameViewportClient::Tick(float DeltaTime)
{
	if (!HasPossessedTarget())
	{
		return false;
	}

	bool bChanged = false;

	// possessed 상태면 마우스 캡처 및 중앙 고정 수행 (스크립트 제어 카메라여도 뷰포트 포커스 유지 목적)
	SetCursorCaptured(true);

	if (bPIEPossessedInputEnabled)
	{
		// Reset accumulators before processing
		MoveInputAccumulator = FVector::ZeroVector;
		LookInputAccumulator = FVector::ZeroVector;

		// Process Enhanced Input
		EnhancedInputManager.ProcessInput(&FInputManager::Get(), DeltaTime);

		// Apply Accumulated Input
		UCameraComponent* TargetCamera = GetPossessedTarget();
		if (TargetCamera)
		{
			// Movement
			if (!MoveInputAccumulator.IsNearlyZero())
			{
				MoveInputAccumulator = MoveInputAccumulator.Normalized();

				FVector FlatForward = TargetCamera->GetForwardVector();
				FVector FlatRight = TargetCamera->GetRightVector();
				FlatForward.Z = 0.0f;
				FlatRight.Z = 0.0f;
				if (!FlatForward.IsNearlyZero()) FlatForward = FlatForward.Normalized();
				if (!FlatRight.IsNearlyZero()) FlatRight = FlatRight.Normalized();

				const float SpeedBoost = bIsSprinting ? InputSettings.SprintMultiplier : 1.0f;
				const FVector WorldDelta = (FlatForward * MoveInputAccumulator.X + FlatRight * MoveInputAccumulator.Y + FVector::UpVector * MoveInputAccumulator.Z)
					* (InputSettings.MoveSpeed * SpeedBoost * DeltaTime);

				TargetCamera->SetWorldLocation(TargetCamera->GetWorldLocation() + WorldDelta);
				bChanged = true;
			}

			// Look
			if (!LookInputAccumulator.IsNearlyZero())
			{
				FRotator Rotation = TargetCamera->GetRelativeRotation();
				Rotation.Yaw += LookInputAccumulator.X * InputSettings.LookSensitivity;
				Rotation.Pitch = Clamp(
					Rotation.Pitch + LookInputAccumulator.Y * InputSettings.LookSensitivity,
					InputSettings.MinPitch,
					InputSettings.MaxPitch);
				Rotation.Roll = 0.0f;
				TargetCamera->SetRelativeRotation(Rotation);
				bChanged = true;
			}
		}
	}

	// Recenter mouse after processing to allow continuous rotation
	if (bCursorCaptured && OwnerHWnd)
	{
		RECT rect;
		::GetClientRect(OwnerHWnd, &rect);
		POINT center = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
		::ClientToScreen(OwnerHWnd, &center);
		::SetCursorPos(center.x, center.y);
		FInputManager::Get().SetLastMousePos(center);
	}

	return bChanged;
}

void UGameViewportClient::SetupInput()
{
	// Create Actions
	ActionMove = new FInputAction("IA_Move", EInputActionValueType::Axis3D);
	ActionLook = new FInputAction("IA_Look", EInputActionValueType::Axis2D);
	ActionSprint = new FInputAction("IA_Sprint", EInputActionValueType::Bool);

	// Create Mapping Context
	DefaultMappingContext = new FInputMappingContext();
	DefaultMappingContext->ContextName = "IMC_Default";

	// Move Mappings
	// W: Forward (X=1)
	DefaultMappingContext->AddMapping(ActionMove, 'W');
	// S: Backward (X=-1)
	DefaultMappingContext->AddMapping(ActionMove, 'S').Modifiers.push_back(new FModifierScale(FVector(-1, 1, 1)));
	// D: Right (Y=1)
	{
		auto& Mapping = DefaultMappingContext->AddMapping(ActionMove, 'D');
		Mapping.Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
	}
	// A: Left (Y=-1)
	{
		auto& Mapping = DefaultMappingContext->AddMapping(ActionMove, 'A');
		Mapping.Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));
		Mapping.Modifiers.push_back(new FModifierScale(FVector(1, -1, 1)));
	}
	// E / Space: Up (Z=1)
	{
		auto& Mapping = DefaultMappingContext->AddMapping(ActionMove, 'E');
		Mapping.Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	}
	DefaultMappingContext->AddMapping(ActionMove, VK_SPACE).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));

	// Q / Ctrl: Down (Z=-1)
	{
		auto& Mapping = DefaultMappingContext->AddMapping(ActionMove, 'Q');
		Mapping.Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
		Mapping.Modifiers.push_back(new FModifierScale(FVector(1, 1, -1)));
	}
	DefaultMappingContext->AddMapping(ActionMove, VK_CONTROL).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::ZYX));
	DefaultMappingContext->Mappings.back().Modifiers.push_back(new FModifierScale(FVector(1, 1, -1)));

	// Look Mappings
	// MouseX -> Yaw
	DefaultMappingContext->AddMapping(ActionLook, static_cast<int32>(EInputKey::MouseX));
	// MouseY -> Pitch (Needs YXZ swizzle to map to Axis2D.Y)
	DefaultMappingContext->AddMapping(ActionLook, static_cast<int32>(EInputKey::MouseY)).Modifiers.push_back(new FModifierSwizzleAxis(FModifierSwizzleAxis::ESwizzleOrder::YXZ));

	// Sprint Mappings
	DefaultMappingContext->AddMapping(ActionSprint, VK_SHIFT);

	//Add Context to Manager
	EnhancedInputManager.AddMappingContext(DefaultMappingContext, 0);

	// Bind Actions
	EnhancedInputManager.BindAction(ActionMove, ETriggerEvent::Triggered, [this](const FInputActionValue& Value) { OnMove(Value); });
	EnhancedInputManager.BindAction(ActionLook, ETriggerEvent::Triggered, [this](const FInputActionValue& Value) { OnLook(Value); });
	EnhancedInputManager.BindAction(ActionSprint, ETriggerEvent::Started, [this](const FInputActionValue& Value) { OnSprintStarted(Value); });
	EnhancedInputManager.BindAction(ActionSprint, ETriggerEvent::Completed, [this](const FInputActionValue& Value) { OnSprintCompleted(Value); });
	EnhancedInputManager.BindAction(ActionSprint, ETriggerEvent::Canceled, [this](const FInputActionValue& Value) { OnSprintCompleted(Value); });
}

void UGameViewportClient::OnMove(const FInputActionValue& Value)
{
	MoveInputAccumulator = MoveInputAccumulator + Value.GetVector();
}

void UGameViewportClient::OnLook(const FInputActionValue& Value)
{
	LookInputAccumulator = LookInputAccumulator + Value.GetVector();
}

void UGameViewportClient::OnSprintStarted(const FInputActionValue& Value)
{
	bIsSprinting = true;
}

void UGameViewportClient::OnSprintCompleted(const FInputActionValue& Value)
{
	bIsSprinting = false;
}

void UGameViewportClient::SetCursorCaptured(bool bCaptured)
{
	if (bCursorCaptured == bCaptured)
	{
		if (bCaptured)
		{
			ApplyCursorClip();
		}
		return;
	}

	bCursorCaptured = bCaptured;
	if (bCursorCaptured)
	{
		while (::ShowCursor(FALSE) >= 0) {}
		ApplyCursorClip();
		return;
	}

	while (::ShowCursor(TRUE) < 0) {}
	::ClipCursor(nullptr);
}

void UGameViewportClient::ApplyCursorClip()
{
	if (!OwnerHWnd)
	{
		return;
	}

	RECT ClientRect = {};
	if (bHasCursorClipRect)
	{
		ClientRect = CursorClipClientRect;
	}
	else if (!::GetClientRect(OwnerHWnd, &ClientRect))
	{
		return;
	}

	POINT TopLeft = { ClientRect.left, ClientRect.top };
	POINT BottomRight = { ClientRect.right, ClientRect.bottom };
	if (!::ClientToScreen(OwnerHWnd, &TopLeft) || !::ClientToScreen(OwnerHWnd, &BottomRight))
	{
		return;
	}

	RECT ScreenRect = { TopLeft.x, TopLeft.y, BottomRight.x, BottomRight.y };
	if (ScreenRect.right > ScreenRect.left && ScreenRect.bottom > ScreenRect.top)
	{
		::ClipCursor(&ScreenRect);
	}
}

