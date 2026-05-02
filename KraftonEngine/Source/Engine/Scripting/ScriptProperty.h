#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class FArchive;

// Script Component의 Property로 나올 수 있는 자료형 후보
enum class EScriptPropertyType : uint8
{
	Bool,
	Int,
	Float,
	String,
	Vector
};

// Lua property 값은 타입별 저장 공간을 모두 들고 있고, Type 값으로 실제 의미를 구분한다.
struct FScriptPropertyValue
{
	EScriptPropertyType Type = EScriptPropertyType::Float;

	bool BoolValue = false;
	int32 IntValue = 0;
	float FloatValue = 0.0f;
	FString StringValue;
	FVector VectorValue = FVector(0.0f, 0.0f, 0.0f);
};

// DeclareProperties({ ... }) 한 항목을 C++ 에디터가 이해할 수 있는 설명으로 옮긴 형태다.
struct FScriptPropertyDesc
{
	FString Name;
	EScriptPropertyType Type = EScriptPropertyType::Float;
	FScriptPropertyValue DefaultValue;
	bool bHasDefault = false;

	bool bHasMin = false;
	bool bHasMax = false;
	float Min = 0.0f;
	float Max = 0.0f;
};

class FScriptProperty
{
public:
	// Lua 파일을 property 스캔 전용 환경에서 실행해 DeclareProperties 내용만 읽는다.
	// 반환값 false는 파일 읽기/실행 실패이고, true + OutDescs empty는 정상적으로 property가 0개라는 뜻이다.
	static bool LoadDescs(const FString& ScriptPath, TArray<FScriptPropertyDesc>& OutDescs, FString& OutError);
	static bool TryParseType(const FString& TypeName, EScriptPropertyType& OutType);
	static FScriptPropertyValue MakeDefaultValue(EScriptPropertyType Type);
	static bool IsTypeCompatible(const FScriptPropertyValue& Value, EScriptPropertyType Type);
	static bool AreValuesEqual(const FScriptPropertyValue& Left, const FScriptPropertyValue& Right);
};

FArchive& operator<<(FArchive& Ar, FScriptPropertyValue& Value);
FArchive& operator<<(FArchive& Ar, FScriptPropertyDesc& Desc);
