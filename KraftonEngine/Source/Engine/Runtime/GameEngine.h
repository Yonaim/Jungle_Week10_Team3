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

protected:
	void LoadStartLevel();
};
