#include "ShapeComponent.h"
#include "GameFramework/World.h"

DEFINE_CLASS(UShapeComponent, UPrimitiveComponent)
HIDE_FROM_COMPONENT_LIST(UShapeComponent)

void UShapeComponent::PostEditProperty(const char* PropertyName) {
	USceneComponent::PostEditProperty(PropertyName);
}

void UShapeComponent::ContributeSelectedVisuals(FScene& Scene) const {
	DrawDebugShape(Scene);
}

void UShapeComponent::DrawDebugRing(FVector Center, float Radius, FVector AxisA, FVector AxisB, uint32 Segments, bool Half, FScene& Scene) const {
	const float Step = 2.0f * FMath::Pi / static_cast<float>(Segments);
	FVector Prev = Center + AxisA * Radius;

	int32 It = Half ? Segments / 2 : Segments;
	for (int32 Index = 1; Index <= It; ++Index)
	{
		const float Angle = Step * static_cast<float>(Index);
		const FVector Next = Center + (AxisA * cosf(Angle) + AxisB * sinf(Angle)) * Radius;
		Scene.AddDebugLine(Prev, Next, ShapeColor);
		Prev = Next;
	}
}