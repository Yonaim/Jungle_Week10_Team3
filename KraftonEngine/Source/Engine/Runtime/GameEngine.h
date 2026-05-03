#pragma once

#include "Engine/Runtime/Engine.h"

class UGameEngine : public UEngine
{
public:
	DECLARE_CLASS(UGameEngine, UEngine)

	UGameEngine() = default;
	~UGameEngine() override = default;

	void Init(FWindowsWindow* InWindow) override;
	void BeginPlay() override;
	bool LoadScene(const FString& InSceneReference) override;

protected:
	void LoadStartLevel();
};
