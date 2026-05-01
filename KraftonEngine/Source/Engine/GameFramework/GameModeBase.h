#pragma once
#include "AActor.h"

class AGameModeBase : public AActor
{
public:
	DECLARE_CLASS(AGameModeBase, AActor);

	virtual void StartPlay() {}
};