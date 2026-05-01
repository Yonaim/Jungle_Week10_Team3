#pragma once
#include "ShapeComponent.h"

class UBoxComponent : public UShapeComponent {
public:
	DECLARE_CLASS(UBoxComponent, UShapeComponent)
	UBoxComponent() = default;
	UBoxComponent(FVector InExtent) : BoxExtent(InExtent) {}
	FVector GetBoxExtent() const { return BoxExtent; }
	void	SetBoxExtent(FVector InExtent) { BoxExtent = InExtent; }
	void	UpdateWorldAABB() const override;
	void	DrawDebugShape(FScene& Scene) const override;
	void	SetRelativeScale(const FVector& NewScale) override;
	void	GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

private:
	FVector BoxExtent = FVector(1.f, 1.f, 1.f);
};