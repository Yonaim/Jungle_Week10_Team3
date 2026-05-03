#pragma once

#include "GameFramework/AActor.h"

class UTextRenderComponent;

class AScreenTextActor : public AActor
{
public:
	DECLARE_CLASS(AScreenTextActor, AActor)

	AScreenTextActor() = default;

	void InitDefaultComponents();

private:
	UTextRenderComponent* TextRenderComponent = nullptr;
};
