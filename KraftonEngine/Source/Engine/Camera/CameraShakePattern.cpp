#include "CameraShakePattern.h"

IMPLEMENT_CLASS(UCameraShakePattern, UObject)

UCameraShakePattern::~UCameraShakePattern() {
	if (TransitionCurveX) {
		UObjectManager::Get().DestroyObject(TransitionCurveX);
		TransitionCurveX = nullptr;
	}

	if (TransitionCurveY) {
		UObjectManager::Get().DestroyObject(TransitionCurveY);
		TransitionCurveY = nullptr;
	}

	if (TransitionCurveZ) {
		UObjectManager::Get().DestroyObject(TransitionCurveZ);
		TransitionCurveZ = nullptr;
	}

	if (RotationCurveX) {
		UObjectManager::Get().DestroyObject(RotationCurveX);
		RotationCurveX = nullptr;
	}

	if (RotationCurveY) {
		UObjectManager::Get().DestroyObject(RotationCurveY);
		RotationCurveY = nullptr;
	}

	if (RotationCurveZ) {
		UObjectManager::Get().DestroyObject(RotationCurveZ);
		RotationCurveZ = nullptr;
	}
}

float UCameraShakePattern::EvalTransitionX(float Alpha) const {
	return TransitionCurveX != nullptr ? TransitionCurveX->Evaluate(Alpha) : 0.f;
}

float UCameraShakePattern::EvalTransitionY(float Alpha) const {
	return TransitionCurveY != nullptr ? TransitionCurveY->Evaluate(Alpha) : 0.f;
}

float UCameraShakePattern::EvalTransitionZ(float Alpha) const {
	return TransitionCurveZ != nullptr ? TransitionCurveZ->Evaluate(Alpha) : 0.f;
}

float UCameraShakePattern::EvalRotationX(float Alpha) const {
	return RotationCurveX != nullptr ? RotationCurveX->Evaluate(Alpha) : 0.f;
}

float UCameraShakePattern::EvalRotationY(float Alpha) const {
	return RotationCurveY != nullptr ? RotationCurveY->Evaluate(Alpha) : 0.f;
}

float UCameraShakePattern::EvalRotationZ(float Alpha) const {
	return RotationCurveZ != nullptr ? RotationCurveZ->Evaluate(Alpha) : 0.f;
}