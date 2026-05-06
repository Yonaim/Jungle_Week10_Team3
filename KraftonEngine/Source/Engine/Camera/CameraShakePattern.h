#pragma once
#include "Math/CurveFloat.h"
#include "Object/ObjectFactory.h"

class UCameraShakePattern  : public UObject {
public:
	DECLARE_CLASS(UCameraShakePattern, UObject)

	// Accessors
	UCurveFloat* GetTransitionCurveX() const { return TransitionCurveX != nullptr ? TransitionCurveX : nullptr; }
	virtual void SetTransitionCurveX(UCurveFloat* InCurve) { TransitionCurveX = InCurve; }
	UCurveFloat* GetTransitionCurveY() const { return TransitionCurveY != nullptr ? TransitionCurveY : nullptr; }
	virtual void SetTransitionCurveY(UCurveFloat* InCurve) { TransitionCurveY = InCurve; }
	UCurveFloat* GetTransitionCurveZ() const { return TransitionCurveZ != nullptr ? TransitionCurveZ : nullptr; }
	virtual void SetTransitionCurveZ(UCurveFloat* InCurve) { TransitionCurveZ = InCurve; }

	UCurveFloat* GetRotationCurveX() const { return RotationCurveX != nullptr ? RotationCurveX : nullptr; }
	virtual void SetRotationCurveX(UCurveFloat* InCurve) { RotationCurveX = InCurve; }
	UCurveFloat* GetRotationCurveY() const { return RotationCurveY != nullptr ? RotationCurveY : nullptr; }
	virtual void SetRotationCurveY(UCurveFloat* InCurve) { RotationCurveY = InCurve; }
	UCurveFloat* GetRotationCurveZ() const { return RotationCurveZ != nullptr ? RotationCurveZ : nullptr; }
	virtual void SetRotationCurveZ(UCurveFloat* InCurve) { RotationCurveZ = InCurve; }

	// Evaluators
	// Evaluate Curve value at Alpha = [0, 1]. Fall back to 0.f if null
	float EvalTransitionX(float Alpha) const;
	float EvalTransitionY(float Alpha) const;
	float EvalTransitionZ(float Alpha) const;

	float EvalRotationX(float Alpha) const;
	float EvalRotationY(float Alpha) const;
	float EvalRotationZ(float Alpha) const;

private:
	UCurveFloat* TransitionCurveX = nullptr;
	UCurveFloat* TransitionCurveY = nullptr;
	UCurveFloat* TransitionCurveZ = nullptr;
	UCurveFloat* RotationCurveX = nullptr;
	UCurveFloat* RotationCurveY = nullptr;
	UCurveFloat* RotationCurveZ = nullptr;
};