#pragma once
#include "Math/Vector.h"

class AActor;
class UPrimitiveComponent;

struct FHitResult {
	FVector Location;
	FVector ImpactLocation;
	FVector ImpactNormal;

	UPrimitiveComponent* Component = nullptr;
	AActor* GetActor();
};

struct FOverlapInfo {
	FHitResult HitResult;
};