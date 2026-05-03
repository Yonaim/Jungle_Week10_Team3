#include "ARunnerGameMode.h"

IMPLEMENT_CLASS(ARunnerGameMode, AGameModeBase)

ARunnerGameMode::ARunnerGameMode()
{
	DefaultPawnClassName = "ARunner";
	PlayerControllerClassName = "APlayerController";
}
