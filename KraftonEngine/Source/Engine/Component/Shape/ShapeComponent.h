#pragma once
#include "Component/PrimitiveComponent.h"

class UShapeComponent : public UPrimitiveComponent {
public:
	DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)
	virtual ~UShapeComponent() = default;
	FPrimitiveSceneProxy* CreateSceneProxy() override { return nullptr; }	// Do not draw debug AABB lines
	void ContributeSelectedVisuals(FScene& Scene) const override;
	void PostEditProperty(const char* PropertyName) override;

public:
	FColor	ShapeColor;
	bool	bDrawOnlyIfSelected = false;

protected:
	virtual void DrawDebugShape(FScene& Scene) const = 0;
	void DrawDebugRing(FVector Center, float Radius, FVector AxisA, FVector AxisB, uint32 Segment, bool Half, FScene& Scene) const;

protected:

};