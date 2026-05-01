#include "BoxComponent.h"
#include "GameFramework/World.h"

IMPLEMENT_CLASS(UBoxComponent, UShapeComponent)

void UBoxComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) {
	USceneComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Box Extent", EPropertyType::Vec3, &BoxExtent, 0.0f, 0.0f, 0.1f });
}

void UBoxComponent::DrawDebugShape(FScene& Scene) const {
	FVector Center = GetWorldLocation();
	FVector X      = GetForwardVector().Normalized() * BoxExtent.X;
	FVector Y      = GetRightVector().Normalized()   * BoxExtent.Y;
	FVector Z      = GetUpVector().Normalized()      * BoxExtent.Z;

	FVector P[8] = {
		Center - X - Y - Z,
		Center + X - Y - Z,
		Center + X + Y - Z,
		Center - X + Y - Z,
		Center - X - Y + Z,
		Center + X - Y + Z,
		Center + X + Y + Z,
		Center - X + Y + Z,
	};

	Scene.AddDebugLine(P[0], P[1], ShapeColor);
	Scene.AddDebugLine(P[1], P[2], ShapeColor);
	Scene.AddDebugLine(P[2], P[3], ShapeColor);
	Scene.AddDebugLine(P[3], P[0], ShapeColor);
	Scene.AddDebugLine(P[4], P[5], ShapeColor);
	Scene.AddDebugLine(P[5], P[6], ShapeColor);
	Scene.AddDebugLine(P[6], P[7], ShapeColor);
	Scene.AddDebugLine(P[7], P[4], ShapeColor);
	Scene.AddDebugLine(P[0], P[4], ShapeColor);
	Scene.AddDebugLine(P[1], P[5], ShapeColor);
	Scene.AddDebugLine(P[2], P[6], ShapeColor);
	Scene.AddDebugLine(P[3], P[7], ShapeColor);
}

void UBoxComponent::SetRelativeScale(const FVector& NewScale) {
	USceneComponent::SetRelativeScale(NewScale);
	BoxExtent = NewScale;
}