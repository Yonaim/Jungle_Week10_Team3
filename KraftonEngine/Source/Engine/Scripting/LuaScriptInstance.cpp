#include "Scripting/LuaScriptInstance.h"

#include "Component/ScriptComponent.h"
#include "Core/Log.h"
#include "Engine/Input/InputSystem.h"
#include "GameFramework/AActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
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
#include <iterator>

namespace
{
	bool TryParseVirtualKey(const FString& KeyName, int& OutVirtualKey);
}

// ====================================================================
// Lua Script 실행 상태 컨테이너
// - FLuaScriptInstance.h에 sol2, Lua, 코루틴, 환경 같은 세부 구현을 노출X
// ====================================================================
struct FLuaScriptInstance::FInstanceImpl
{
	enum class ELuaWaitKind
	{
		None,
		Time,
		Frames,
		MoveDone,
		KeyDown,
		Signal,
		AnimationDone
	};

	struct FLuaWaitCondition
	{
		ELuaWaitKind Kind = ELuaWaitKind::None;
		float TimeRemaining = 0.0f;
		int FramesRemaining = 0;
		FString Name;
		FString KeyName;
	};

	// 실행 중이거나 다음 프레임에 다시 resume될 coroutine 상태를 묶는다.
	struct FRunningCoroutine
	{
		FString FunctionName;			// 코루틴 시작된 함수 이름
		// sol::thread는 OS thread가 아니라 Lua coroutine이 사용할 별도 Lua stack/context이다.
		// sol::coroutine만 보관하면 그 coroutine이 의존하는 Lua thread 수명이 끊길 수 있으므로 둘을 함께 보관한다.
		sol::thread Thread;
		sol::coroutine Coroutine;
		FLuaWaitCondition Wait;
	};

	UScriptComponent* OwnerComponent = nullptr;
	FString ScriptPath;

	// 로딩/에러 상태
	bool bLoaded = false;
	bool bHasError = false;
	FString LastError;

	FLuaActorProxy OwnerProxy;
	TArray<FLuaActorProxy> ManagedProxies;
	TSet<FString> PendingSignals;
	float LastDeltaTime = 0.0f;
	float ElapsedTime = 0.0f;

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

	FLuaActorProxy TrackProxy(const FLuaActorProxy& Proxy)
	{
		// spawn_actor/find_actor가 반환한 Proxy도 MoveToActor 같은 Lua task를 수행할 수 있다.
		// Lua에는 Proxy 값이 복사되어 전달되므로, C++ 인스턴스가 같은 공유 TaskState를 가진 Proxy를 보관하고 Tick해야 작업이 계속 진행된다.
		if (Proxy.IsValid())
		{
			ManagedProxies.push_back(Proxy);
		}

		return Proxy;
	}

	void TickLuaProxyTasks(float DeltaTime)
	{
		// OwnerProxy는 obj로 노출되는 기본 허브이며 항상 먼저 갱신한다.
		OwnerProxy.TickLuaTasks(DeltaTime);

		for (size_t Index = 0; Index < ManagedProxies.size();)
		{
			FLuaActorProxy& Proxy = ManagedProxies[Index];
			if (!Proxy.IsValid())
			{
				// Lua가 Actor를 소유하지 않으므로 World가 Actor를 파괴하면 Proxy 목록에서도 정리한다.
				ManagedProxies.erase(ManagedProxies.begin() + Index);
				continue;
			}

			Proxy.TickLuaTasks(DeltaTime);
			++Index;
		}
	}

	bool ApplyYieldResult(FLuaScriptInstance* OwnerInstance, FRunningCoroutine& Entry, sol::protected_function_result& Result)
	{
		// 코루틴이 yield한 값은 Lua 실행을 실제로 잠들게 하는 것이 아니라,
		// C++ Scheduler가 다음 resume 시점을 판단할 수 있는 WaitCondition 명령으로 변환된다.
		Entry.Wait = FLuaWaitCondition();

		if (Result.return_count() <= 0)
		{
			Entry.Wait.Kind = ELuaWaitKind::None;
			return true;
		}

		const sol::type YieldType = Result.get_type(0);
		if (YieldType == sol::type::number)
		{
			// 기존 호환을 위해 coroutine.yield(1.0) 또는 과거 Wait 구현의 숫자 yield도 시간 대기로 해석한다.
			Entry.Wait.Kind = ELuaWaitKind::Time;
			Entry.Wait.TimeRemaining = (std::max)(0.0f, Result.get<float>());
			return true;
		}

		if (YieldType != sol::type::table)
		{
			OwnerInstance->SetError("Coroutine '" + Entry.FunctionName + "' yielded unsupported value.");
			return false;
		}

		sol::table Command = Result.get<sol::table>();
		const FString Type = Command["type"].get_or(FString());

		if (Type == "time")
		{
			Entry.Wait.Kind = ELuaWaitKind::Time;
			Entry.Wait.TimeRemaining = (std::max)(0.0f, Command["seconds"].get_or(0.0f));
			return true;
		}

		if (Type == "frames")
		{
			Entry.Wait.Kind = ELuaWaitKind::Frames;
			Entry.Wait.FramesRemaining = (std::max)(0, Command["frames"].get_or(0));
			return true;
		}

		if (Type == "move_done")
		{
			Entry.Wait.Kind = ELuaWaitKind::MoveDone;
			return true;
		}

		if (Type == "key_down")
		{
			Entry.Wait.Kind = ELuaWaitKind::KeyDown;
			Entry.Wait.KeyName = Command["key"].get_or(FString());
			return true;
		}

		if (Type == "signal")
		{
			Entry.Wait.Kind = ELuaWaitKind::Signal;
			Entry.Wait.Name = Command["name"].get_or(FString());
			return true;
		}

		if (Type == "anim_done")
		{
			Entry.Wait.Kind = ELuaWaitKind::AnimationDone;
			Entry.Wait.Name = Command["name"].get_or(FString());
			return true;
		}

		OwnerInstance->SetError("Coroutine '" + Entry.FunctionName + "' yielded unknown wait command: " + Type);
		return false;
	}

	bool ShouldResumeCoroutine(FRunningCoroutine& Entry, float DeltaTime)
	{
		// WaitCondition마다 resume 가능 조건이 다르므로 한 곳에서 판단한다.
		// 이 함수가 false를 반환하면 해당 coroutine만 대기하고 게임 전체 Tick은 계속 진행된다.
		switch (Entry.Wait.Kind)
		{
		case ELuaWaitKind::None:
			return true;
		case ELuaWaitKind::Time:
			Entry.Wait.TimeRemaining -= DeltaTime;
			return Entry.Wait.TimeRemaining <= 0.0f;
		case ELuaWaitKind::Frames:
			--Entry.Wait.FramesRemaining;
			return Entry.Wait.FramesRemaining <= 0;
		case ELuaWaitKind::MoveDone:
			return OwnerProxy.IsMoveDone();
		case ELuaWaitKind::KeyDown:
		{
			int VirtualKey = 0;
			if (!TryParseVirtualKey(Entry.Wait.KeyName, VirtualKey))
			{
				return false;
			}

			return InputSystem::Get().GetKeyDown(VirtualKey);
		}
		case ELuaWaitKind::Signal:
			return PendingSignals.find(Entry.Wait.Name) != PendingSignals.end();
		case ELuaWaitKind::AnimationDone:
			return OwnerProxy.IsAnimationDone(Entry.Wait.Name);
		default:
			return true;
		}
	}

	bool HandleProtectedResult(FLuaScriptInstance* OwnerInstance, const FString& Context, sol::protected_function_result&& Result)
	{
		// 일반 BeginPlay/Tick/EndPlay 호출 경로에서는 yield를 허용하지 않는다.
		// lifecycle 함수가 yield되면 호출자가 재개 시점을 알 수 없어 엔진 생명주기와 Lua 실행 상태가 어긋나므로,
		// 대기가 필요하면 반드시 StartCoroutine/start_coroutine으로 시작한 coroutine 안에서 wait 계열 함수를 호출해야 한다.
		if (Result.status() == sol::call_status::yielded)
		{
			OwnerInstance->SetError(Context + ": unexpected yield. Use wait/Wait only inside StartCoroutine coroutines.");
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

	sol::object FindLuaObjectByPath(sol::environment& Env, const FString& Path)
	{
		// Env["EnemyAI.start"]는 Lua table 내부 함수를 찾지 못한다.
		// dot path를 세그먼트별로 따라가야 EnemyAI.start 같은 네임스페이스형 coroutine entry를 지원할 수 있다.
		if (Path.empty())
		{
			return sol::lua_nil;
		}

		size_t SegmentStart = 0;
		size_t Dot = Path.find('.');
		FString Segment = Path.substr(0, Dot);
		sol::object Current = Env[Segment];

		while (Dot != FString::npos)
		{
			if (!Current.valid() || Current.get_type() != sol::type::table)
			{
				return sol::lua_nil;
			}

			sol::table Table = Current.as<sol::table>();
			SegmentStart = Dot + 1;
			Dot = Path.find('.', SegmentStart);
			Segment = Path.substr(
				SegmentStart,
				Dot == FString::npos ? FString::npos : Dot - SegmentStart);
			Current = Table[Segment];
		}

		return Current;
	}

	AActor* SpawnActorByClassName(UWorld* World, const FString& ClassName)
	{
		if (!World || ClassName.empty())
		{
			return nullptr;
		}

		// 현재 엔진의 범용 생성 경로는 FObjectFactory + World::AddActor 조합이다.
		// World가 AddActor를 호출해야 BeginPlay/Octree/Picking 등 World 소유 생명주기 등록이 함께 일어난다.
		UObject* NewObject = FObjectFactory::Get().Create(ClassName, World);
		AActor* NewActor = Cast<AActor>(NewObject);
		if (!NewActor)
		{
			if (NewObject)
			{
				UObjectManager::Get().DestroyObject(NewObject);
			}
			return nullptr;
		}

		if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(NewActor))
		{
			// AStaticMeshActor는 생성자에서 RootComponent를 만들지 않으므로,
			// World::AddActor가 PIE에서 BeginPlay를 즉시 호출할 수 있기 전에 기본 컴포넌트를 먼저 만든다.
			// 이 순서를 지켜야 BeginPlay 시점에 스크립트와 렌더링이 유효한 RootComponent를 볼 수 있다.
			StaticMeshActor->InitDefaultComponents();
		}

		World->AddActor(NewActor);
		return NewActor;
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
	BindDebugTimeFunctions();
	BindWorldFunctions();
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
	Impl->ManagedProxies.clear();
	Impl->PendingSignals.clear();
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
	Impl->ManagedProxies.clear();
	Impl->PendingSignals.clear();
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
	BindDebugTimeFunctions();
	BindWorldFunctions();

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
	// TODO: 필요한 경우 hot-reload에서 Lua environment와 coroutine state를 보존하는 경로를 별도로 설계한다.
	// 지금은 reload 중 이전 Env와 Lua stack을 유지하면 C++ Proxy/Actor 생명주기와 어긋날 수 있어 안전하게 모두 정리한다.
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
	if (Impl)
	{
		// delta_time() 전역 함수는 현재 프레임 값을 반환해야 하므로 Lua Tick 호출 전에 최신 값을 저장한다.
		Impl->LastDeltaTime = DeltaTime;
		Impl->ElapsedTime += (std::max)(0.0f, DeltaTime);
	}

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

	sol::object FunctionObject = FindLuaObjectByPath(Impl->Env, FunctionName);
	if (!FunctionObject.valid() || FunctionObject.get_type() != sol::type::function)
	{
		SetError("StartCoroutine failed: missing Lua function '" + FunctionName + "'.");
		return false;
	}

	sol::protected_function Function = FunctionObject.as<sol::protected_function>();
	sol::state& Lua = FLuaScriptRuntime::Get().GetLuaState();

	// coroutine마다 별도 Lua thread를 만들어 서로의 yield/resume 상태를 분리한다.
	// sol::thread는 OS thread가 아니라 Lua coroutine 실행을 위한 별도 Lua stack/context이며,
	// FRunningCoroutine에 thread와 coroutine을 함께 보관해야 Lua thread lifetime 문제가 생기지 않는다.
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
		// wait 계열 함수는 실제로 C++를 대기시키지 않고 yield command만 반환한다.
		// 여기서 command를 WaitCondition으로 바꿔 저장해야 이후 Tick에서 resume 가능 여부를 판단할 수 있다.
		if (!Impl->ApplyYieldResult(this, CoroutineEntry, StartResult))
		{
			return false;
		}

		if (CoroutineEntry.Coroutine.runnable())
		{
			Impl->EnqueueCoroutine(std::move(CoroutineEntry));
		}
	}

	return true;
}

void FLuaScriptInstance::TickCoroutines(float DeltaTime)
{
	if (!Impl)
	{
		return;
	}

	Impl->LastDeltaTime = DeltaTime;
	Impl->TickLuaProxyTasks(DeltaTime);
	Impl->bTickingCoroutines = true;

	// coroutine 배열을 직접 순회하면서 완료/실패한 항목은 즉시 제거한다
	for (size_t Index = 0; Index < Impl->Coroutines.size();)
	{
		FInstanceImpl::FRunningCoroutine& CoroutineEntry = Impl->Coroutines[Index];
		if (!Impl->ShouldResumeCoroutine(CoroutineEntry, DeltaTime))
		{
			++Index;
			continue;
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
			// resume된 coroutine이 다시 yield하면 다음 대기 조건을 새로 저장한다.
			// table command를 쓰면 시간뿐 아니라 입력, signal, 이동 완료 같은 게임 조건도 같은 scheduler에서 처리할 수 있다.
			if (!Impl->ApplyYieldResult(this, CoroutineEntry, ResumeResult))
			{
				Impl->Coroutines.erase(Impl->Coroutines.begin() + Index);
				continue;
			}

			++Index;
			continue;
		}

		Impl->Coroutines.erase(Impl->Coroutines.begin() + Index);
	}

	Impl->bTickingCoroutines = false;
	Impl->FlushPendingCoroutines();
	// signal은 한 프레임짜리 이벤트로 처리한다.
	// clear를 늦추면 다음 프레임 코루틴이 과거 signal을 잘못 소비할 수 있다.
	Impl->PendingSignals.clear();
}

void FLuaScriptInstance::StopAllCoroutines()
{
	if (!Impl)
	{
		return;
	}

	Impl->Coroutines.clear();
	Impl->PendingCoroutines.clear();
	Impl->PendingSignals.clear();
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

	Impl->Env.set_function("start_coroutine", [this](const FString& FunctionName)
	{
		return StartCoroutine(FunctionName);
	});

	// wait 함수들은 실제로 C++ thread를 sleep하지 않는다.
	// Lua coroutine에서 table command를 yield하고, C++ Scheduler가 그 command를 WaitCondition으로 바꿔 다음 resume 시점을 결정한다.
	auto MakeWaitCommand = [this](const FString& Type)
	{
		// table 자체는 Lua VM에 생성하고 environment를 통해 yield한다.
		// sol::environment는 lookup 범위일 뿐 table factory가 아니므로 Runtime의 state를 사용한다.
		sol::table Command = FLuaScriptRuntime::Get().GetLuaState().create_table();
		Command["type"] = Type;
		return Command;
	};

	Impl->Env.set_function("wait", sol::yielding([MakeWaitCommand](float Seconds)
	{
		sol::table Command = MakeWaitCommand("time");
		Command["seconds"] = (std::max)(0.0f, Seconds);
		return Command;
	}));

	Impl->Env.set_function("Wait", sol::yielding([MakeWaitCommand](float Seconds)
	{
		sol::table Command = MakeWaitCommand("time");
		Command["seconds"] = (std::max)(0.0f, Seconds);
		return Command;
	}));

	Impl->Env.set_function("wait_frames", sol::yielding([MakeWaitCommand](int Frames)
	{
		sol::table Command = MakeWaitCommand("frames");
		Command["frames"] = (std::max)(0, Frames);
		return Command;
	}));

	Impl->Env.set_function("wait_until_move_done", sol::yielding([MakeWaitCommand]()
	{
		return MakeWaitCommand("move_done");
	}));

	Impl->Env.set_function("wait_key_down", sol::yielding([MakeWaitCommand](const FString& KeyName)
	{
		sol::table Command = MakeWaitCommand("key_down");
		Command["key"] = KeyName;
		return Command;
	}));

	Impl->Env.set_function("wait_signal", sol::yielding([MakeWaitCommand](const FString& SignalName)
	{
		sol::table Command = MakeWaitCommand("signal");
		Command["name"] = SignalName;
		return Command;
	}));

	Impl->Env.set_function("wait_anim_done", sol::yielding([MakeWaitCommand](const FString& AnimName)
	{
		sol::table Command = MakeWaitCommand("anim_done");
		Command["name"] = AnimName;
		return Command;
	}));

	Impl->Env.set_function("signal", [this](const FString& SignalName)
	{
		if (!Impl || SignalName.empty())
		{
			return;
		}

		// signal은 코루틴 사이의 한 프레임 이벤트이므로 PendingSignals에 잠시 기록하고 TickCoroutines 끝에서 제거한다.
		Impl->PendingSignals.insert(SignalName);
	});
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
		return TryParseVirtualKey(KeyName, VirtualKey) ? InputSystem::Get().GetKey(VirtualKey) : false;
	});

	Impl->Env.set_function("GetKeyDown", [](const FString& KeyName)
	{
		int VirtualKey = 0;
		return TryParseVirtualKey(KeyName, VirtualKey) ? InputSystem::Get().GetKeyDown(VirtualKey) : false;
	});

	Impl->Env.set_function("GetKeyUp", [](const FString& KeyName)
	{
		int VirtualKey = 0;
		return TryParseVirtualKey(KeyName, VirtualKey) ? InputSystem::Get().GetKeyUp(VirtualKey) : false;
	});

	Impl->Env.set_function("key", [](const FString& KeyName)
	{
		int VirtualKey = 0;
		return TryParseVirtualKey(KeyName, VirtualKey) ? InputSystem::Get().GetKey(VirtualKey) : false;
	});

	Impl->Env.set_function("key_down", [](const FString& KeyName)
	{
		int VirtualKey = 0;
		return TryParseVirtualKey(KeyName, VirtualKey) ? InputSystem::Get().GetKeyDown(VirtualKey) : false;
	});

	Impl->Env.set_function("key_up", [](const FString& KeyName)
	{
		int VirtualKey = 0;
		return TryParseVirtualKey(KeyName, VirtualKey) ? InputSystem::Get().GetKeyUp(VirtualKey) : false;
	});
}

void FLuaScriptInstance::BindDebugTimeFunctions()
{
	if (!Impl)
	{
		return;
	}

	auto LogLuaMessage = [](const char* Prefix, sol::variadic_args Args)
	{
		FString Message;
		for (auto Arg : Args)
		{
			if (!Message.empty())
			{
				Message += " ";
			}

			Message += Arg.as<FString>();
		}

		UE_LOG("%s %s", Prefix, Message.c_str());
	};

	Impl->Env.set_function("log", [LogLuaMessage](sol::variadic_args Args)
	{
		LogLuaMessage("[Lua]", Args);
	});

	Impl->Env.set_function("warn", [LogLuaMessage](sol::variadic_args Args)
	{
		LogLuaMessage("[Lua][Warn]", Args);
	});

	Impl->Env.set_function("error_log", [LogLuaMessage](sol::variadic_args Args)
	{
		LogLuaMessage("[Lua][Error]", Args);
	});

	Impl->Env.set_function("print", [LogLuaMessage](sol::variadic_args Args)
	{
		// print도 인스턴스 환경에서 엔진 로그로 보내면 WinMain 환경에서도 Lua 로그를 놓치지 않는다.
		LogLuaMessage("[Lua]", Args);
	});

	Impl->Env.set_function("time", [this]()
	{
		return Impl ? Impl->ElapsedTime : 0.0f;
	});

	Impl->Env.set_function("delta_time", [this]()
	{
		return Impl ? Impl->LastDeltaTime : 0.0f;
	});
}

void FLuaScriptInstance::BindWorldFunctions()
{
	if (!Impl)
	{
		return;
	}

	Impl->Env.set_function("spawn_actor", [this](const FString& ClassName, const FVector& Location)
	{
		UScriptComponent* OwnerComponent = GetOwnerComponent();
		AActor* OwnerActor = GetOwnerActor();
		UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
		if (!OwnerComponent || !OwnerActor || !World)
		{
			return FLuaActorProxy();
		}

		AActor* SpawnedActor = SpawnActorByClassName(World, ClassName);
		if (!SpawnedActor)
		{
			return FLuaActorProxy();
		}

		SpawnedActor->SetActorLocation(Location);

		// Actor 생성과 소유권은 World/Object system이 담당한다.
		// Lua는 새 Actor를 직접 소유하지 않고 Proxy만 받으므로, Destroy/World 종료 이후에도 Proxy 함수는 생존 체크로 안전하게 실패한다.
		return Impl->TrackProxy(MakeActorProxy(SpawnedActor));
	});

	Impl->Env.set_function("find_actor", [this](const FString& ActorName)
	{
		AActor* OwnerActor = GetOwnerActor();
		UWorld* World = OwnerActor ? OwnerActor->GetWorld() : nullptr;
		if (!World)
		{
			return FLuaActorProxy();
		}

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor || !IsAliveObject(Actor))
			{
				continue;
			}

			if (Actor->GetFName().ToString() == ActorName)
			{
				return Impl->TrackProxy(MakeActorProxy(Actor));
			}
		}

		return FLuaActorProxy();
	});

	Impl->Env.set_function("destroy_actor", [](FLuaActorProxy& ActorProxy)
	{
		// destroy_actor는 Lua가 Actor를 소유한다는 뜻이 아니라 Proxy의 Destroy 래퍼일 뿐이다.
		ActorProxy.Destroy();
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
