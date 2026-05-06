#pragma once

#include "Core/CoreTypes.h"

// Sol.hpp에 있는 Check 매크로 겹침 방지 목적 제거
#pragma region SolInclude
#ifdef check
#pragma push_macro("check")
#undef check
#define LUA_COROUTINE_SCHEDULER_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define LUA_COROUTINE_SCHEDULER_RESTORE_CHECKF_MACRO
#endif

#include "sol/sol.hpp"

// Sol.hpp include 완료 후 복구
#ifdef LUA_COROUTINE_SCHEDULER_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_COROUTINE_SCHEDULER_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_COROUTINE_SCHEDULER_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_COROUTINE_SCHEDULER_RESTORE_CHECK_MACRO
#endif
#pragma endregion

#include <string>
#include <memory>
#include <unordered_set>
#include <vector>

struct FLuaActorProxy;
class FLuaScriptInstance;

class FLuaCoroutineScheduler
{
public:
	enum class EWaitKind
	{
		None,
		Time,
		RealTime,
		Frames,
		MoveDone,
		KeyDown,
		Signal
	};

	struct FWaitCondition
	{
		EWaitKind Kind = EWaitKind::None;
		float TimeRemaining = 0.0f;
		int FramesRemaining = 0;
		FString Name;
		FString KeyName;
	};

	struct FRunningCoroutine
	{
		FString DebugName;
		uint64 Id = 0;

		sol::thread Thread;
		sol::protected_function EntryFunction;
		sol::coroutine Coroutine;

		FWaitCondition Wait;
		bool bCancelRequested = false;
	};

public:
	void Initialize(
		FLuaScriptInstance* InOwner,
		sol::state* InLua,
		sol::environment* InEnv,
		FLuaActorProxy* InOwnerProxy);

	void BindToEnvironment();

	bool StartCoroutine(const FString& DebugName, sol::protected_function Function);

	void Tick(float DeltaTime, float RawDeltaTime);

	void StopAll();

	void Signal(const FString& Name);

private:
	void Enqueue(std::unique_ptr<FRunningCoroutine> Entry);
	void FlushPending();

	bool ShouldResume(FRunningCoroutine& Entry, float DeltaTime, float RawDeltaTime);
	bool ApplyYieldResult(FRunningCoroutine& Entry, sol::protected_function_result& Result);
	bool HandleCoroutineError(FRunningCoroutine& Entry, sol::protected_function_result&& Result);

	sol::table MakeWaitCommand(sol::this_state ThisState, const FString& Type);

private:
	FLuaScriptInstance* Owner = nullptr;
	sol::state* Lua = nullptr;
	sol::environment* Env = nullptr;
	FLuaActorProxy* OwnerProxy = nullptr;

	std::vector<std::unique_ptr<FRunningCoroutine>> Running;
	std::vector<std::unique_ptr<FRunningCoroutine>> Pending;

	std::unordered_set<std::string> ActiveSignals;
	std::unordered_set<std::string> PendingSignals;

	bool bTicking = false;
	uint64 NextCoroutineId = 1;
};
