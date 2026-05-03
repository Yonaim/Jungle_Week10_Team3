#pragma once
#include "ObstacleActorBase.h"

class ASimpleObstacleActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(ASimpleObstacleActor, AObstacleActorBase)

private:
	void OnPlayerCollision() override {}
};