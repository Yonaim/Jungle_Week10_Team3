#pragma once

#include "GameFramework/AActor.h"

class USceneComponent;
class UBillboardComponent;
class UCameraComponent;

class APawnActor : public AActor
{
public:
	DECLARE_CLASS(APawnActor, AActor)

	APawnActor();

	void BeginPlay() override;

	void InitDefaultComponents();

	// PIE/Game에서 배치된 Pawn만 있어도 조종 가능한 기본 카메라를 보장한다.
	UCameraComponent* GetOrCreateGameplayCameraComponent();
	UCameraComponent* GetGameplayCameraComponent() const { return GameplayCameraComponent; }

private:
	USceneComponent* RootSceneComponent = nullptr;
	UBillboardComponent* BillboardComponent = nullptr;
	UCameraComponent* GameplayCameraComponent = nullptr;
};
