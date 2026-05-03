#pragma once
#include "ObstacleActorBase.h"

class AMustJumpObstacleActor : public AObstacleActorBase {
public:
	DECLARE_CLASS(AMustJumpObstacleActor, AObstacleActorBase)
	void InitDefaultComponents(const FString& UStaticMeshFileName) override;
	void OnPlayerCollision() override {}

private:


};