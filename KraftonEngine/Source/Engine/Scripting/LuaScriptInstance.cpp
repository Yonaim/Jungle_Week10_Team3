#include "Scripting/LuaScriptInstance.h"

#include "Component/ScriptComponent.h"
#include "Core/Log.h"
#include "Engine/Input/InputManager.h"
#include "GameFramework/AActor.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"

// Sol.hpp에 있는 Check 매크로 겹침 방지 목적 제거
#ifdef check
#pragma push_macro("check")
#undef check
#define LUA_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define LUA_RESTORE_CHECKF_MACRO
#endif

#include "sol/sol.hpp"

// Sol.hpp include 완료 후 복구
#ifdef LUA_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_RESTORE_CHECK_MACRO
#endif

#include <algorithm>
#include <cctype>
#include <functional>

// ====================================================================
// Lua Script 실행 상태 컨테이너
// - FLuaScriptInstance.h에 sol2, Lua, 코루틴, 환경 같은 세부 구현을 노출X
// ====================================================================
struct FLuaScriptInstance::FInstanceImpl
{
	// 실행 중이거나 다음 프레임에 다시 resume될 coroutine 상태를 묶는다.
	struct FRunningCoroutine
	{
		FString FunctionName;			// 에러난 함수 이름
		sol::thread Thread;				// Lua Thread
		sol::coroutine Coroutine;		// Resume할 Lua Coroutine 객체
		float WaitRemaining = 0.0f;		// Wait까지 남은 시간
	};

	UScriptComponent* OwnerComponent = nullptr;
	FString ScriptPath;

	// 로딩/에러 상태
	bool bLoaded = false;
	bool bHasError = false;
	FString LastError;

	FLuaActorProxy OwnerProxy;

	sol::environment Env;
	// Instance에서 default로 캐싱할 목록
	sol::protected_function FnBeginPlay;
	sol::protected_function FnTick;
	sol::protected_function FnEndPlay;

	TArray<FRunningCoroutine> Coroutines;
	TArray<FRunningCoroutine> PendingCoroutines;
	bool bTickingCoroutines = false;

	void ClearFunctionCache()
	{
		// 스크립트가 다시 로드되면 이전 environment에서 꺼낸 함수 캐시를 버려야 한다.
		FnBeginPlay = sol::protected_function();
		FnTick = sol::protected_function();
		FnEndPlay = sol::protected_function();
	}

	void ResetEnvironment(sol::state& Lua)
	{
		// 매 스크립트 인스턴스는 전역 Lua VM 위에 독립 environment를 만든다.
		Env = sol::environment(Lua, sol::create, Lua.globals());
		ClearFunctionCache();
	}

	void EnqueueCoroutine(FRunningCoroutine&& CoroutineEntry)
	{
		// Tick 중 새 coroutine이 시작되면 현재 순회 중인 배열을 건드리지 않도록 지연 추가한다.
		if (bTickingCoroutines)
		{
			PendingCoroutines.push_back(std::move(CoroutineEntry));
			return;
		}

		Coroutines.push_back(std::move(CoroutineEntry));
	}

	void FlushPendingCoroutines()
	{
		// Tick 도중 밀어둔 coroutine을 순회 종료 후 한 번에 합친다.
		if (PendingCoroutines.empty())
		{
			return;
		}

		Coroutines.insert(
			Coroutines.end(),
			std::make_move_iterator(PendingCoroutines.begin()),
			std::make_move_iterator(PendingCoroutines.end()));
		PendingCoroutines.clear();
	}

	bool HandleProtectedResult(FLuaScriptInstance* OwnerInstance, const FString& Context, sol::protected_function_result&& Result)
	{
		// 일반 함수 호출 경로에서는 yield를 허용하지 않는다
		// yield는 StartCoroutine으로 시작한 coroutine 안에서만 유효하다
		if (Result.status() == sol::call_status::yielded)
		{
			OwnerInstance->SetError(Context + ": unexpected yield. Use Wait(seconds) only inside StartCoroutine coroutines.");
			return false;
		}

		if (Result.valid())
		{
			return true;
		}

		const sol::optional<sol::error> MaybeError = Result.get<sol::optional<sol::error>>();
		const FString ErrorMessage = MaybeError ? MaybeError->what() : FString("Unknown Lua error.");
		OwnerInstance->SetError(Context + ": " + ErrorMessage);
		return false;
	}

	template <typename... TArgs>
	bool CallFunction(FLuaScriptInstance* OwnerInstance, const FString& FunctionName, sol::protected_function& Function, TArgs&&... Args)
	{
		// 함수가 정의되지 않은 경우는 스크립트 에러로 보지 않고 조용히 통과시킨다.
		if (!Function.valid())
		{
			return true;
		}

		sol::protected_function_result Result = Function(std::forward<TArgs>(Args)...);
		return HandleProtectedResult(OwnerInstance, FunctionName, std::move(Result));
	}
};

// TODO: Input System 생기면 지워야함.
namespace
{
	// 키매핑
	bool TryParseVirtualKey(const FString& KeyName, int& OutVirtualKey)
	{
		// 입력 바인딩은 단순 문자열 기반 인터페이스를 Lua에 노출하고,
		// 실제 Win32 virtual key 변환은 C++ 쪽에서 처리한다.
		if (KeyName.empty())
		{
			return false;
		}

		FString UpperKey = KeyName;
		std::transform(
			UpperKey.begin(),
			UpperKey.end(),
			UpperKey.begin(),
			[](unsigned char Character)
			{
				return static_cast<char>(std::toupper(Character));
			});

		if (UpperKey.size() == 1)
		{
			OutVirtualKey = static_cast<unsigned char>(UpperKey[0]);
			return true;
		}

		if (UpperKey == "SPACE")
		{
			OutVirtualKey = VK_SPACE;
			return true;
		}

		if (UpperKey == "ESC" || UpperKey == "ESCAPE")
		{
			OutVirtualKey = VK_ESCAPE;
			return true;
		}

		if (UpperKey == "LEFT")
		{
			OutVirtualKey = VK_LEFT;
			return true;
		}

		if (UpperKey == "RIGHT")
		{
			OutVirtualKey = VK_RIGHT;
			return true;
		}

		if (UpperKey == "UP")
		{
			OutVirtualKey = VK_UP;
			return true;
		}

		if (UpperKey == "DOWN")
		{
			OutVirtualKey = VK_DOWN;
			return true;
		}

		if (UpperKey == "CTRL" || UpperKey == "CONTROL")
		{
			OutVirtualKey = VK_CONTROL;
			return true;
		}

		if (UpperKey == "SHIFT")
		{
			OutVirtualKey = VK_SHIFT;
			return true;
		}

		if (UpperKey == "ALT")
		{
			OutVirtualKey = VK_MENU;
			return true;
		}

		return false;
	}

	void CacheFunction(sol::environment& Env, const char* Name, sol::protected_function& OutFunction)
	{
		// BeginPlay/Tick/EndPlay 같은 선택적 엔트리 포인트를 미리 캐시해
		// 매 프레임 문자열 lookup을 반복하지 않게 한다.
		sol::object Object = Env[Name];
		if (Object.valid() && Object.get_type() == sol::type::function)
		{
			OutFunction = Object.as<sol::protected_function>();
			return;
		}

		OutFunction = sol::protected_function();
	}
}

FLuaScriptInstance::FLuaScriptInstance()
	: Impl(std::make_unique<FInstanceImpl>())
{
}

FLuaScriptInstance::~FLuaScriptInstance()
{
	Shutdown();
}

bool FLuaScriptInstance::Initialize(UScriptComponent* InOwnerComponent)
{
	// 이전 owner/state가 남아 있다면 먼저 완전히 끊고 다시 시작한다.
	Shutdown();

	if (!InOwnerComponent)
	{
		SetError("LuaScriptInstance requires a valid owner component.");
		return false;
	}

	if (!FLuaScriptRuntime::Get().bIsInitialized())
	{
		SetError(FLuaScriptRuntime::Get().GetLastError());
		return false;
	}

	Impl->OwnerComponent = InOwnerComponent;
	Impl->OwnerProxy = MakeActorProxy(GetOwnerActor());
	Impl->ResetEnvironment(FLuaScriptRuntime::Get().GetLuaState());

	// Initialize는 environment 골격과 기본 바인딩만 준비하고,
	// 실제 스크립트 파일 실행은 LoadFromFile에서 한다.
	ClearError();
	BindOwnerObject();
	BindCoroutineFunctions();
	BindInputFunctions();
	return true;
}

void FLuaScriptInstance::Shutdown()
{
	if (!Impl)
	{
		return;
	}

	// Owner 참조, environment, coroutine 상태를 모두 끊어
	// 다음 Initialize/LoadFromFile이 완전히 새 상태에서 시작하게 만든다.
	StopAllCoroutines();
	Impl->ClearFunctionCache();
	Impl->Env = sol::environment();
	Impl->OwnerProxy = FLuaActorProxy();
	Impl->OwnerComponent = nullptr;
	Impl->ScriptPath.clear();
	Impl->bLoaded = false;
	ClearError();
}

bool FLuaScriptInstance::LoadFromFile(const FString& InScriptPath)
{
	if (!Impl->OwnerComponent)
	{
		SetError("LuaScriptInstance is not initialized.");
		return false;
	}

	if (!FLuaScriptRuntime::Get().bIsInitialized())
	{
		SetError(FLuaScriptRuntime::Get().GetLastError());
		return false;
	}

	ClearError();
	StopAllCoroutines();
	Impl->bLoaded = false;

	// Instance는 더 이상 Scripts/ prefix 규칙이나 파일 위치 정책을 직접 알지 않는다.
	// 저장 문자열은 FScriptPaths가 canonical한 내부 표기로 맞춘다.
	Impl->ScriptPath = FScriptPaths::NormalizeScriptPath(InScriptPath);

	// reload 시에도 이전 global, 함수 캐시, coroutine이 남지 않도록
	// 매번 environment를 처음부터 다시 구성한다.
	Impl->OwnerProxy = MakeActorProxy(GetOwnerActor());
	Impl->ResetEnvironment(FLuaScriptRuntime::Get().GetLuaState());
	BindOwnerObject();
	BindCoroutineFunctions();
	BindInputFunctions();

	FString ScriptSource;
	FString FileReadError;
	if (!FScriptPaths::ReadScriptFile(Impl->ScriptPath, ScriptSource, FileReadError))
	{
		SetError(FileReadError);
		return false;
	}

	const std::filesystem::path ResolvedPath = FScriptPaths::ResolveScriptPath(Impl->ScriptPath);

	// Lua chunk name은 디버깅과 에러 메시지에서 실제 파일 위치가 보이는 쪽이 유리해서
	// 정규화 상대 경로 대신 resolve된 절대 경로를 사용한다.
	const FString ChunkName = FPaths::ToUtf8(ResolvedPath.generic_wstring());
	sol::protected_function_result LoadResult =
		FLuaScriptRuntime::Get().GetLuaState().safe_script(
			ScriptSource, 
			Impl->Env, 
			// 로드 실패 시 예외를 삼키지 말고 protected result로 받아서
			// component 쪽 에러 UI에 그대로 전달한다.
			sol::script_pass_on_error,
			ChunkName, 
			sol::load_mode::text);

	if (!Impl->HandleProtectedResult(this, "LoadScript(" + Impl->ScriptPath + ")", std::move(LoadResult)))
	{
		return false;
	}

	CacheFunction(Impl->Env, "BeginPlay", Impl->FnBeginPlay);
	CacheFunction(Impl->Env, "Tick", Impl->FnTick);
	CacheFunction(Impl->Env, "EndPlay", Impl->FnEndPlay);

	// 엔트리 포인트 캐시가 끝난 시점부터 이 인스턴스를 로드 완료로 본다.
	Impl->bLoaded = true;
	return true;
}

bool FLuaScriptInstance::Reload()
{
	if (Impl->ScriptPath.empty())
	{
		SetError("ReloadScript failed: ScriptPath is empty.");
		return false;
	}

	StopAllCoroutines();
	return LoadFromFile(Impl->ScriptPath);
}

bool FLuaScriptInstance::IsLoaded() const
{
	return Impl && Impl->bLoaded;
}

void FLuaScriptInstance::SetScriptPath(const FString& InScriptPath)
{
	if (!Impl)
	{
		Impl = std::make_unique<FInstanceImpl>();
	}

	Impl->ScriptPath = FScriptPaths::NormalizeScriptPath(InScriptPath);
}

const FString& FLuaScriptInstance::GetScriptPath() const
{
	static const FString EmptyPath;
	return Impl ? Impl->ScriptPath : EmptyPath;
}

bool FLuaScriptInstance::CallBeginPlay()
{
	return Impl->CallFunction(this, "BeginPlay", Impl->FnBeginPlay);
}

bool FLuaScriptInstance::CallTick(float DeltaTime)
{
	return Impl->CallFunction(this, "Tick", Impl->FnTick, DeltaTime);
}

bool FLuaScriptInstance::CallEndPlay()
{
	return Impl->CallFunction(this, "EndPlay", Impl->FnEndPlay);
}

bool FLuaScriptInstance::StartCoroutine(const FString& FunctionName)
{
	if (!Impl || !Impl->bLoaded)
	{
		SetError("StartCoroutine failed: script is not loaded.");
		return false;
	}

	sol::object FunctionObject = Impl->Env[FunctionName];
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		SetError("StartCoroutine failed: missing Lua function '" + FunctionName + "'.");
		return false;
	}

	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::state& Lua = FLuaScriptRuntime::Get().GetLuaState();

	// coroutine마다 별도 Lua thread를 만들어 서로의 yield/resume 상태를 분리한다.
	sol::thread Thread = sol::thread::create(Lua.lua_state());
	sol::state_view ThreadState = Thread.state();
	constexpr const char* CoroutineEntryName = "__script_coroutine_entry";
	ThreadState[CoroutineEntryName] = Function;

	FInstanceImpl::FRunningCoroutine CoroutineEntry;
	CoroutineEntry.FunctionName = FunctionName;
	CoroutineEntry.Thread = std::move(Thread);
	CoroutineEntry.Coroutine = ThreadState[CoroutineEntryName];
	ThreadState[CoroutineEntryName] = sol::lua_nil;

	sol::protected_function_result StartResult = CoroutineEntry.Coroutine();
	if (!StartResult.valid() && StartResult.status() != sol::call_status::yielded)
	{
		return Impl->HandleProtectedResult(this, "Coroutine(" + FunctionName + ")", std::move(StartResult));
	}

	if (StartResult.status() == sol::call_status::yielded)
	{
		// Wait(seconds)는 number 하나를 yield한다고 가정하고,
		// 그 값을 다음 resume까지 남은 시간으로 저장한다.
		float WaitSeconds = 0.0f;
		if (StartResult.return_count() > 0)
		{
			if (StartResult.get_type(0) != sol::type::number)
			{
				SetError("Coroutine '" + FunctionName + "' yielded an unsupported value. Use Wait(seconds).");
				return false;
			}

			WaitSeconds = StartResult.get<float>();
		}

		CoroutineEntry.WaitRemaining = (std::max)(0.0f, WaitSeconds);
		if (CoroutineEntry.Coroutine.runnable())
		{
			Impl->EnqueueCoroutine(std::move(CoroutineEntry));
		}
	}

	return true;
}

void FLuaScriptInstance::TickCoroutines(float DeltaTime)
{
	if (!Impl || Impl->Coroutines.empty())
	{
		return;
	}

	Impl->bTickingCoroutines = true;

	// coroutine 배열을 직접 순회하면서 완료/실패한 항목은 즉시 제거한다.
	for (size_t Index = 0; Index < Impl->Coroutines.size();)
	{
		FInstanceImpl::FRunningCoroutine& CoroutineEntry = Impl->Coroutines[Index];
		if (CoroutineEntry.WaitRemaining > 0.0f)
		{
			CoroutineEntry.WaitRemaining -= DeltaTime;
			if (CoroutineEntry.WaitRemaining > 0.0f)
			{
				++Index;
				continue;
			}
		}

		sol::protected_function_result ResumeResult = CoroutineEntry.Coroutine();
		if (!ResumeResult.valid() && ResumeResult.status() != sol::call_status::yielded)
		{
			Impl->HandleProtectedResult(this, "Coroutine(" + CoroutineEntry.FunctionName + ")", std::move(ResumeResult));
			Impl->Coroutines.erase(Impl->Coroutines.begin() + Index);
			continue;
		}

		if (ResumeResult.status() == sol::call_status::yielded)
		{
			// 재-yield한 coroutine은 다음 대기 시간만 갱신하고 유지한다.
			float WaitSeconds = 0.0f;
			if (ResumeResult.return_count() > 0)
			{
				if (ResumeResult.get_type(0) != sol::type::number)
				{
					SetError("Coroutine '" + CoroutineEntry.FunctionName + "' yielded an unsupported value. Use Wait(seconds).");
					Impl->Coroutines.erase(Impl->Coroutines.begin() + Index);
					continue;
				}

				WaitSeconds = ResumeResult.get<float>();
			}

			CoroutineEntry.WaitRemaining = (std::max)(0.0f, WaitSeconds);
			++Index;
			continue;
		}

		Impl->Coroutines.erase(Impl->Coroutines.begin() + Index);
	}

	Impl->bTickingCoroutines = false;
	Impl->FlushPendingCoroutines();
}

void FLuaScriptInstance::StopAllCoroutines()
{
	if (!Impl)
	{
		return;
	}

	Impl->Coroutines.clear();
	Impl->PendingCoroutines.clear();
	Impl->bTickingCoroutines = false;
}


bool FLuaScriptInstance::HasError() const
{
	return Impl && Impl->bHasError;
}

const FString& FLuaScriptInstance::GetLastError() const
{
	static const FString EmptyError;
	return Impl ? Impl->LastError : EmptyError;
}

void FLuaScriptInstance::ClearError()
{
	if (!Impl)
	{
		return;
	}

	Impl->bHasError = false;
	Impl->LastError.clear();
}

UScriptComponent* FLuaScriptInstance::GetOwnerComponent() const
{
	return (Impl && Impl->OwnerComponent && IsAliveObject(Impl->OwnerComponent)) ? Impl->OwnerComponent : nullptr;
}

AActor* FLuaScriptInstance::GetOwnerActor() const
{
	UScriptComponent* OwnerComponent = GetOwnerComponent();
	return OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
}

void FLuaScriptInstance::BindOwnerObject()
{
	if (!Impl)
	{
		return;
	}

	// Lua 스크립트에서는 obj를 통해 owner actor 기능에 접근한다.
	Impl->OwnerProxy = MakeActorProxy(GetOwnerActor());
	Impl->Env["obj"] = std::ref(Impl->OwnerProxy);
}

void FLuaScriptInstance::BindCoroutineFunctions()
{
	if (!Impl)
	{
		return;
	}

	Impl->Env.set_function("StartCoroutine", [this](const FString& FunctionName)
	{
		return StartCoroutine(FunctionName);
	});

	// Wait는 coroutine 안에서만 yield되어야 하므로 yielding 함수로 바인딩한다.
	Impl->Env.set_function("Wait", sol::yielding([](float Seconds)
	{
		return (std::max)(0.0f, Seconds);
	}));
}

void FLuaScriptInstance::BindInputFunctions()
{
	if (!Impl)
	{
		return;
	}

	// 입력 바인딩은 문자열 기반 API로 노출해서
	// Lua 스크립트가 엔진 키코드 상수를 직접 알 필요 없게 만든다.
	Impl->Env.set_function("GetKey", [](const FString& KeyName)
	{
		int VirtualKey = 0;
		return TryParseVirtualKey(KeyName, VirtualKey) ? FInputManager::Get().IsKeyDown(VirtualKey) : false;
	});

	Impl->Env.set_function("GetKeyDown", [](const FString& KeyName)
	{
		int VirtualKey = 0;
		return TryParseVirtualKey(KeyName, VirtualKey) ? FInputManager::Get().IsKeyPressed(VirtualKey) : false;
	});

	Impl->Env.set_function("GetKeyUp", [](const FString& KeyName)
	{
		int VirtualKey = 0;
		return TryParseVirtualKey(KeyName, VirtualKey) ? FInputManager::Get().IsKeyReleased(VirtualKey) : false;
	});

	// Mouse Delta & Wheel
	Impl->Env.set_function("GetMouseDeltaX", []() { return FInputManager::Get().GetMouseDeltaX(); });
	Impl->Env.set_function("GetMouseDeltaY", []() { return FInputManager::Get().GetMouseDeltaY(); });
	Impl->Env.set_function("GetMouseWheel", []() { return FInputManager::Get().GetMouseWheelDelta(); });
	Impl->Env.set_function("MouseMoved", []() { return FInputManager::Get().MouseMoved(); });

	// Dragging
	Impl->Env.set_function("IsDragging", [](const FString& ButtonName)
	{
		int VirtualKey = 0;
		if (TryParseVirtualKey(ButtonName, VirtualKey))
			return FInputManager::Get().IsDragging(VirtualKey);
		return false;
	});

	Impl->Env.set_function("GetDragDeltaX", [](const FString& ButtonName)
	{
		int VirtualKey = 0;
		if (TryParseVirtualKey(ButtonName, VirtualKey))
			return (float)FInputManager::Get().GetDragDelta(VirtualKey).x;
		return 0.0f;
	});

	Impl->Env.set_function("GetDragDeltaY", [](const FString& ButtonName)
	{
		int VirtualKey = 0;
		if (TryParseVirtualKey(ButtonName, VirtualKey))
			return (float)FInputManager::Get().GetDragDelta(VirtualKey).y;
		return 0.0f;
	});

	Impl->Env.set_function("GetDragDistance", [](const FString& ButtonName)
	{
		int VirtualKey = 0;
		if (TryParseVirtualKey(ButtonName, VirtualKey))
			return FInputManager::Get().GetDragDistance(VirtualKey);
		return 0.0f;
	});
}

FLuaActorProxy FLuaScriptInstance::MakeActorProxy(AActor* Actor) const
{
	FLuaActorProxy Proxy;
	// Lua 쪽에 죽은 UObject 포인터가 넘어가지 않도록 살아 있는 actor만 노출한다.
	Proxy.Actor = (Actor && IsAliveObject(Actor)) ? Actor : nullptr;
	return Proxy;
}

void FLuaScriptInstance::SetError(const FString& ErrorMessage)
{
	if (!Impl)
	{
		Impl = std::make_unique<FInstanceImpl>();
	}

	Impl->bHasError = true;
	Impl->LastError = ErrorMessage;
	UE_LOG("[LuaScript] %s", ErrorMessage.c_str());
}
