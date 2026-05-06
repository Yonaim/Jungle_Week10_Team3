#include "MinimalViewInfo.h"

float FPostProcessSettings::GetScalar(FName Name, float DefaultValue) const
{
	return GetParameterValue(ScalarParameter, Name, DefaultValue);
}

void FPostProcessSettings::SetScalar(FName Name, float Value)
{
	SetParameterValue(ScalarParameter, Name, Value);
}

FVector FPostProcessSettings::GetVector(FName Name, FVector DefaultValue) const
{
	return GetParameterValue(VectorParameter, Name, DefaultValue);
}

void FPostProcessSettings::SetVector(FName Name, FVector Value)
{
	SetParameterValue(VectorParameter, Name, Value);
}

FLinearColor FPostProcessSettings::GetColor(FName Name, FLinearColor DefaultValue) const
{
	return GetParameterValue(ColorParameter, Name, DefaultValue);
}

void FPostProcessSettings::SetColor(FName Name, FLinearColor Value)
{
	SetParameterValue(ColorParameter, Name, Value);
}

