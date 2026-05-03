#pragma once
#include "ObstacleActorBase.h"

class AVerticalWireActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(AVerticalWireActor, AObstacleActorBase)
	void InitDefaultComponents(const FString& UStaticMeshFileName) override;
	void OnPlayerCollision() override {}

private:

};