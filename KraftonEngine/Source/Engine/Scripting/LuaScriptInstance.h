#pragma once

#include "Core/CoreTypes.h"
#include "Scripting/LuaScriptRuntime.h"

#include <memory>

class AActor;
class UScriptComponent;

// ======================================================
//  -- Actor의 Lua 실행 상태를 관리하는 Instance 클래스 --
// - 같은 Lua 파일을 써도 Actor마다 obj, 함수, 코루틴, 에러 상태가 따로 필요하기 때문에
// - 런타임 전역 Lua VM 위에 Actor별 environment를 따로 올려서 상태를 격리한다.
// ======================================================
class FLuaScriptInstance
{
public:
	FLuaScriptInstance();
	FLuaScriptInstance(const FLuaScriptInstance&) = delete;
	FLuaScriptInstance& operator=(const FLuaScriptInstance&) = delete;
	~FLuaScriptInstance();

	// OwnerComponent와 Lua environment를 연결하고 실행 준비를 끝낸다.
	bool Initialize(UScriptComponent* InOwnerComponent);

	// 캐시된 함수, environment, coroutine 상태를 모두 정리한다.
	void Shutdown();

	// ScriptPath를 정규화하고 파일을 읽어 environment에 로드한다.
	bool LoadFromFile(const FString& InScriptPath);

	// 마지막으로 성공적으로 설정된 ScriptPath 기준으로 다시 로드한다.
	bool Reload();
	bool IsLoaded() const;

	// 직렬화/에디터/UI에서 받은 경로를 내부 저장용 형태로 맞춘다.
	void SetScriptPath(const FString& InScriptPath);
	const FString& GetScriptPath() const;

	// Lua 스크립트의 생명주기 함수 캐시를 호출한다.
	bool CallBeginPlay();
	bool CallTick(float DeltaTime);
	bool CallEndPlay();

	// 스크립트가 시작한 coroutine을 관리한다.
	bool StartCoroutine(const FString& FunctionName);
	void TickCoroutines(float DeltaTime);
	void StopAllCoroutines();

	// 최근 Lua 실행 에러 상태를 Component가 읽을 수 있게 노출한다.
	bool HasError() const;
	const FString& GetLastError() const;
	void ClearError();

	// 현재 연결된 owner를 안전하게 조회한다.
	UScriptComponent* GetOwnerComponent() const;
	AActor* GetOwnerActor() const;

private:
	// Lua 스크립트에 노출할 바인딩을 environment에 심는다.
	void BindOwnerObject();
	void BindCoroutineFunctions();
	void BindInputFunctions();

	// Lua에 노출할 ActorProxy를 현재 owner actor 기준으로 다시 만든다.
	FLuaActorProxy MakeActorProxy(AActor* Actor) const;

	// 내부 상태에 에러를 저장하고 로그에 남긴다.
	void SetError(const FString& ErrorMessage);

private:
	struct FInstanceImpl;
	std::unique_ptr<FInstanceImpl> Impl;
};
