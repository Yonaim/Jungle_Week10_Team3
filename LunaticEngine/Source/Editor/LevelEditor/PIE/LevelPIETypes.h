#pragma once

#include "Component/CameraComponent.h"
#include "Core/EngineTypes.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Object/FName.h"

enum class EPIESessionDestination : uint8
{
    InProcess,
};

enum class EPIEPlayMode : uint8
{
    PlayInViewport,
    Simulate,
};

enum class EPIEControlMode : uint8
{
    Possessed,
    Ejected
};

struct FRequestPlaySessionParams
{
    EPIESessionDestination SessionDestination = EPIESessionDestination::InProcess;
    EPIEPlayMode PlayMode = EPIEPlayMode::PlayInViewport;
};

struct FPIEViewportCameraSnapshot
{
    FVector Location;
    FRotator Rotation;
    FMinimalViewInfo CameraState;
    bool bValid = false;
};

struct FPlayInEditorSessionInfo
{
    FRequestPlaySessionParams OriginalRequestParams;
    double PIEStartTime = 0.0;
    FName PreviousActiveWorldHandle;
    FPIEViewportCameraSnapshot SavedViewportCamera;
};
