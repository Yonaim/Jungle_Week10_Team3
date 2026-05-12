#include "PCH/LunaticPCH.h"
#include "Transform.h"

FMatrix FTransform::ToMatrix() const
{
	FMatrix translateMatrix = FMatrix::MakeTranslationMatrix(Location);

	FMatrix rotationMatrix = Rotation.ToMatrix();

	FMatrix scaleMatrix = FMatrix::MakeScaleMatrix(Scale);

	return scaleMatrix * rotationMatrix * translateMatrix;
}
