#pragma once
#include "CameraModifier.h"

#include <memory>

class ULuaCameraModifier : public UCameraModifier
{
public:
	DECLARE_CLASS(ULuaCameraModifier, UCameraModifier)

	ULuaCameraModifier();

	bool Initialize(const FString& InScriptPath, const TMap<FString, float>& Params = {});
	bool ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV) override;

	const FString& GetScriptPath() const { return ScriptPath; }
	const FString& GetLastError() const;

protected:
	~ULuaCameraModifier() override;

public:
	FString ScriptPath;

private:
	struct FLuaModifierImpl;
	std::unique_ptr<FLuaModifierImpl> Impl;
};
