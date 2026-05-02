#include "Scripting/LuaScriptRuntime.h"

#include "Component/ActorComponent.h"
#include "Component/ScriptComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"
#include "Object/Object.h"
#include "Platform/DirectoryWatcher.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"

// 이거 넣는 이유: sol/sol.hpp 내장 check하고 충돌 방지 목적
// 내부에서 Sol/Sol.cpp include
#pragma region check 충돌방지
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

#include <sol/sol.hpp>

#ifdef LUA_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_RESTORE_CHECK_MACRO
#endif
#pragma endregion

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

struct FLuaScriptRuntime::FRuntimeImpl
{
	// 런타임이 소유하는 전역 Lua VM.
	// 개별 ScriptInstance는 이 VM 위에 environment만 따로 만든다.
	sol::state Lua;
};

struct FLuaActorProxy::FLuaActorTaskState
{
	// MoveTo/MoveToActor는 Lua 호출 즉시 블로킹하지 않고,
	// C++ Tick에서 목표까지 조금씩 이동하기 위해 상태만 기록한다.
	bool bMoveActive = false;
	bool bMoveToActor = false;
	FVector MoveTargetLocation = FVector(0.0f, 0.0f, 0.0f);
	AActor* MoveTargetActor = nullptr;
	float MoveSpeed = 300.0f;
	float MoveAcceptRadius = 1.0f;

};

namespace
{
	AActor* ResolveAliveActor(AActor* Actor)
	{
		return (Actor && IsAliveObject(Actor)) ? Actor : nullptr;
	}

	UActorComponent* ResolveAliveComponent(UActorComponent* Component)
	{
		if (!Component || !IsAliveObject(Component))
		{
			return nullptr;
		}

		// ComponentProxy는 Component만 살아 있는 것으로 충분하지 않다.
		// Owner Actor가 파괴되었거나 소유 목록에서 빠진 Component라면 Lua 호출이 엔진 내부 상태를 건드리지 못하게 실패시킨다.
		AActor* OwnerActor = ResolveAliveActor(Component->GetOwner());
		if (!OwnerActor)
		{
			return nullptr;
		}

		const TArray<UActorComponent*>& OwnerComponents = OwnerActor->GetComponents();
		const auto It = std::find(OwnerComponents.begin(), OwnerComponents.end(), Component);
		return (It != OwnerComponents.end()) ? Component : nullptr;
	}

	FLuaComponentProxy MakeComponentProxy(UActorComponent* Component)
	{
		FLuaComponentProxy Proxy;
		// Lua에 넘기는 순간에도 한 번 거르지만, Proxy가 복사되어 오래 살아남을 수 있으므로 실제 안전성은 각 함수의 재검증이 책임진다.
		Proxy.Component = ResolveAliveComponent(Component);
		return Proxy;
	}

	FString StripLuaClassPrefix(const FString& Name)
	{
		// C++ class 이름은 UStaticMeshComponent처럼 U prefix를 갖지만,
		// Lua 작성자는 StaticMeshComponent처럼 엔진 접두어를 빼고 쓰는 경우가 많으므로 비교용으로만 완화한다.
		if (Name.size() > 1 && Name[0] == 'U')
		{
			return Name.substr(1);
		}

		return Name;
	}

	bool IsLuaNameMatch(const FString& RequestedName, const FString& CandidateName)
	{
		if (RequestedName.empty() || CandidateName.empty())
		{
			return false;
		}

		return RequestedName == CandidateName
			|| StripLuaClassPrefix(RequestedName) == StripLuaClassPrefix(CandidateName);
	}

	bool IsComponentNameMatch(const UActorComponent* Component, const FString& ComponentName)
	{
		if (!Component || ComponentName.empty())
		{
			return false;
		}

		// GetComponent는 실제 객체 이름과 class 이름을 모두 받는다.
		// 예: "UStaticMeshComponent_0" 같은 UObject 이름, 또는 "StaticMeshComponent" 같은 Lua 친화적 class 이름.
		if (IsLuaNameMatch(ComponentName, Component->GetFName().ToString()))
		{
			return true;
		}

		UClass* ComponentClass = Component->GetClass();
		return ComponentClass && IsLuaNameMatch(ComponentName, ComponentClass->GetName());
	}

	UClass* FindComponentClassByLuaName(const FString& TypeName)
	{
		if (TypeName.empty())
		{
			return nullptr;
		}

		for (UClass* Class : UClass::GetAllClasses())
		{
			if (!Class || !Class->IsA(UActorComponent::StaticClass()))
			{
				continue;
			}

			if (IsLuaNameMatch(TypeName, Class->GetName()))
			{
				return Class;
			}
		}

		return nullptr;
	}

	bool IsLuaScriptFile(const FString& Path)
	{
		// DirectoryWatcher는 prefix 포함 상대 경로를 넘겨주므로
		// 여기서는 단순히 확장자만 보고 Lua 대상인지 빠르게 걸러낸다.
		const std::filesystem::path FilePath(FPaths::ToWide(Path));
		FString Extension = FPaths::ToUtf8(FilePath.extension().wstring());
		std::transform(
			Extension.begin(),
			Extension.end(),
			Extension.begin(),
			[](unsigned char Character)
			{
				return static_cast<char>(std::tolower(Character));
			});
		return Extension == ".lua";
	}
}

// ================================================================
// LuaActorProxy Section
// - 여기에 있는 함수들만 Lua에서 호출 가능
// - 단순 호출만 하면 안되고 bind까지 해줘야함.
// ================================================================

FLuaActorProxy::FLuaActorProxy()
	: TaskState(std::make_shared<FLuaActorTaskState>())
{
}

bool FLuaActorProxy::IsValid() const
{
	// Lua가 들고 있는 Proxy는 Actor를 소유하지 않는다.
	// 따라서 매 호출마다 UObject 생존 목록을 확인해야 hot-reload, Destroy, World 종료 이후의 dangling pointer 접근을 막을 수 있다.
	return GetActor() != nullptr;
}

AActor* FLuaActorProxy::GetActor() const
{
	// Proxy는 실제 AActor*를 Lua에 직접 노출하지 않는 얇은 허브다.
	// 모든 기능은 이 안전 조회를 통과한 뒤에만 실행되므로 죽은 Actor는 조용히 실패한다.
	return ResolveAliveActor(Actor);
}

FString FLuaActorProxy::GetName() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetFName().ToString() : FString();
}

uint32 FLuaActorProxy::GetUUID() const
{
	// Proxy는 항상 안전한 Actor 조회를 거친 뒤에만 실제 기능을 호출한다.
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetUUID() : 0u;
}

FVector FLuaActorProxy::GetLocation() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetActorLocation() : FVector(0.0f, 0.0f, 0.0f);
}

void FLuaActorProxy::SetLocation(const FVector& InLocation)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	TargetActor->SetActorLocation(InLocation);
}

FVector FLuaActorProxy::GetRotation() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetActorRotation().ToVector() : FVector(0.0f, 0.0f, 0.0f);
}

void FLuaActorProxy::SetRotation(const FVector& InRotation)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	// 엔진의 FRotator는 FVector(Roll, Pitch, Yaw) 변환 규칙을 제공하므로 Lua에는 Vector 하나로 단순화해 노출한다.
	TargetActor->SetActorRotation(InRotation);
}

FVector FLuaActorProxy::GetScale() const
{
	AActor* TargetActor = GetActor();
	return TargetActor ? TargetActor->GetActorScale() : FVector(1.0f, 1.0f, 1.0f);
}

void FLuaActorProxy::SetScale(const FVector& InScale)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	TargetActor->SetActorScale(InScale);
}

FVector FLuaActorProxy::GetVelocity() const
{
	return Velocity;
}

void FLuaActorProxy::SetVelocity(const FVector& InVelocity)
{
	Velocity = InVelocity;
}

void FLuaActorProxy::AddWorldOffset(const FVector& Delta)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	TargetActor->AddActorWorldOffset(Delta);
}

void FLuaActorProxy::AddWorldOffsetXYZ(float X, float Y, float Z)
{
	AddWorldOffset(FVector(X, Y, Z));
}

void FLuaActorProxy::MoveTo(const FVector& Target)
{
	if (!TaskState)
	{
		TaskState = std::make_shared<FLuaActorTaskState>();
	}

	// MoveTo는 방향 벡터가 아니라 월드 좌표 목표를 저장한다.
	// 실제 위치 변경은 TickMovement에서 생존 체크 후 처리해야 Destroy된 Actor를 건드리지 않는다.
	TaskState->bMoveActive = true;
	TaskState->bMoveToActor = false;
	TaskState->MoveTargetLocation = Target;
	TaskState->MoveTargetActor = nullptr;
}

void FLuaActorProxy::MoveTo2D(float X, float Y)
{
	const FVector CurrentLocation = GetLocation();
	MoveTo(FVector(X, Y, CurrentLocation.Z));
}

void FLuaActorProxy::MoveTo3D(float X, float Y, float Z)
{
	MoveTo(FVector(X, Y, Z));
}

void FLuaActorProxy::MoveBy(const FVector& Delta)
{
	MoveTo(GetLocation() + Delta);
}

void FLuaActorProxy::MoveBy2D(float X, float Y)
{
	MoveBy(FVector(X, Y, 0.0f));
}

void FLuaActorProxy::MoveBy3D(float X, float Y, float Z)
{
	MoveBy(FVector(X, Y, Z));
}

void FLuaActorProxy::MoveToActor(const FLuaActorProxy& TargetActor)
{
	if (!TaskState)
	{
		TaskState = std::make_shared<FLuaActorTaskState>();
	}

	// Lua에는 raw AActor*를 넘기지 않고 Proxy만 받는다.
	// 단, target Actor는 나중에 파괴될 수 있으므로 여기서 얻은 포인터도 Tick마다 다시 IsAliveObject로 검증한다.
	TaskState->bMoveActive = true;
	TaskState->bMoveToActor = true;
	TaskState->MoveTargetActor = TargetActor.GetActor();
	TaskState->MoveTargetLocation = TargetActor.GetLocation();
}

void FLuaActorProxy::StopMove()
{
	if (!TaskState)
	{
		return;
	}

	// 이동을 멈출 때 target actor 포인터도 같이 지워야 이후 프레임에서 죽은 Actor를 다시 검사하지 않는다.
	TaskState->bMoveActive = false;
	TaskState->bMoveToActor = false;
	TaskState->MoveTargetActor = nullptr;
}

bool FLuaActorProxy::IsMoveDone() const
{
	return !TaskState || !TaskState->bMoveActive;
}

void FLuaActorProxy::SetMoveSpeed(float InSpeed)
{
	if (!TaskState)
	{
		TaskState = std::make_shared<FLuaActorTaskState>();
	}

	// 음수/비정상 속도는 이동 방향을 뒤집거나 NaN을 퍼뜨릴 수 있으므로 0 이상으로 제한한다.
	TaskState->MoveSpeed = (std::max)(0.0f, std::isfinite(InSpeed) ? InSpeed : 0.0f);
}

float FLuaActorProxy::GetMoveSpeed() const
{
	return TaskState ? TaskState->MoveSpeed : 0.0f;
}

FLuaComponentProxy FLuaActorProxy::GetComponent(const FString& ComponentName)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor || ComponentName.empty())
	{
		return FLuaComponentProxy();
	}

	// Lua에는 UActorComponent*를 직접 반환하지 않는다.
	// 이름이 맞는 Component를 찾더라도 반드시 FLuaComponentProxy로 한 번 감싸서 이후 호출마다 생존 체크가 수행되게 한다.
	for (UActorComponent* Component : TargetActor->GetComponents())
	{
		if (!ResolveAliveComponent(Component))
		{
			continue;
		}

		if (IsComponentNameMatch(Component, ComponentName))
		{
			return MakeComponentProxy(Component);
		}
	}

	return FLuaComponentProxy();
}

FLuaComponentProxy FLuaActorProxy::GetComponentByType(const FString& TypeName)
{
	AActor* TargetActor = GetActor();
	if (!TargetActor || TypeName.empty())
	{
		return FLuaComponentProxy();
	}

	UClass* RequestedClass = FindComponentClassByLuaName(TypeName);
	for (UActorComponent* Component : TargetActor->GetComponents())
	{
		if (!ResolveAliveComponent(Component))
		{
			continue;
		}

		UClass* ComponentClass = Component->GetClass();
		if (!ComponentClass)
		{
			continue;
		}

		// 등록된 class를 찾은 경우 상속 관계까지 허용한다.
		// 예를 들어 "PrimitiveComponent" 검색으로 StaticMeshComponent를 찾을 수 있어야 타입 기반 조회로서 자연스럽다.
		if (RequestedClass)
		{
			if (ComponentClass->IsA(RequestedClass))
			{
				return MakeComponentProxy(Component);
			}
			continue;
		}

		// class registry에서 못 찾은 이름은 exact class 이름 비교만 fallback으로 수행한다.
		// 오타나 아직 로드되지 않은 타입이 Component 포인터 노출로 이어지지 않도록 실패 시 빈 Proxy를 돌려준다.
		if (IsLuaNameMatch(TypeName, ComponentClass->GetName()))
		{
			return MakeComponentProxy(Component);
		}
	}

	return FLuaComponentProxy();
}

FLuaComponentProxy FLuaActorProxy::GetScriptComponent()
{
	// Script 전용 Proxy가 생기기 전까지는 공통 ComponentProxy로 반환한다.
	// TODO: ScriptComponent 전용 Lua API가 필요해지면 FLuaScriptComponentProxy로 분리한다.
	return GetComponentByType("ScriptComponent");
}

FLuaComponentProxy FLuaActorProxy::GetStaticMeshComponent()
{
	// StaticMesh 전용 기능(SetVisible, SetMaterial 등)은 아직 노출하지 않고, 이번 단계에서는 공통 활성화/소유자 API까지만 제공한다.
	// TODO: Mesh 조작 요구가 확정되면 FLuaStaticMeshComponentProxy를 추가해 전용 함수만 선별 노출한다.
	return GetComponentByType("StaticMeshComponent");
}

void FLuaActorProxy::TickLuaTasks(float DeltaTime)
{
	// ActorProxy는 Lua 스크립팅의 기본 허브이므로, Lua 전용 비동기 작업을 한 곳에서 갱신한다.
	TickMovement(DeltaTime);
}

void FLuaActorProxy::TickMovement(float DeltaTime)
{
	if (!TaskState || !TaskState->bMoveActive)
	{
		return;
	}

	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		// 자기 Actor가 죽었으면 Lua Proxy가 더 이상 할 수 있는 일이 없으므로 이동 상태를 정리한다.
		StopMove();
		return;
	}

	FVector TargetLocation = TaskState->MoveTargetLocation;
	if (TaskState->bMoveToActor)
	{
		AActor* MoveTargetActor = ResolveAliveActor(TaskState->MoveTargetActor);
		if (!MoveTargetActor)
		{
			// MoveToActor는 대상 Actor가 언제든 Destroy될 수 있다.
			// 그래서 매 프레임 생존 여부를 다시 확인하고, 죽었다면 안전하게 이동을 중단한다.
			StopMove();
			return;
		}

		// 대상 Actor가 움직일 수 있으므로 시작 시점 위치를 고정하지 않고 매 프레임 최신 위치를 목표로 사용한다.
		TargetLocation = MoveTargetActor->GetActorLocation();
		TaskState->MoveTargetLocation = TargetLocation;
	}

	const FVector CurrentLocation = TargetActor->GetActorLocation();
	const FVector ToTarget = TargetLocation - CurrentLocation;
	const float Distance = ToTarget.Length();
	if (Distance <= TaskState->MoveAcceptRadius)
	{
		TargetActor->SetActorLocation(TargetLocation);
		StopMove();
		return;
	}

	const float Step = TaskState->MoveSpeed * (std::max)(0.0f, DeltaTime);
	if (Step <= 0.0f)
	{
		return;
	}

	if (Step >= Distance)
	{
		TargetActor->SetActorLocation(TargetLocation);
		StopMove();
		return;
	}

	FVector Direction = ToTarget;
	Direction.Normalize();
	TargetActor->SetActorLocation(CurrentLocation + Direction * Step);
}

void FLuaActorProxy::PrintLocation() const
{
	const FVector Location = GetLocation();
	UE_LOG("[Lua] Actor %u Location = (%.3f, %.3f, %.3f)", GetUUID(), Location.X, Location.Y, Location.Z);
}

void FLuaActorProxy::Destroy()
{
	// Lua에서 Destroy를 호출해도 실제 파괴는 World를 통해 진행한다.
	AActor* TargetActor = GetActor();
	if (!TargetActor)
	{
		return;
	}

	UWorld* World = TargetActor->GetWorld();
	if (!World)
	{
		return;
	}

	World->DestroyActor(TargetActor);
	Actor = nullptr;
	StopMove();
}

// ================================================================
// LuaComponentProxy Section
// - Component raw pointer는 Lua로 노출하지 않고, 이 Proxy를 통해 허용된 기능만 제공한다.
// - Component와 Owner Actor는 언제든 파괴될 수 있으므로 모든 함수가 안전 조회를 먼저 수행한다.
// ================================================================

bool FLuaComponentProxy::IsValid() const
{
	// Proxy는 Component를 소유하지 않는 약한 참조다.
	// Component 자체와 Owner Actor가 모두 살아 있고, 여전히 Owner의 소유 목록에 있을 때만 유효하다고 본다.
	return GetComponent() != nullptr;
}

UActorComponent* FLuaComponentProxy::GetComponent() const
{
	// Lua에 전달된 Proxy가 오래 보관된 뒤 호출될 수 있으므로 매번 UObject 생존 목록과 Owner 관계를 다시 확인한다.
	return ResolveAliveComponent(Component);
}

FString FLuaComponentProxy::GetName() const
{
	UActorComponent* TargetComponent = GetComponent();
	return TargetComponent ? TargetComponent->GetFName().ToString() : FString();
}

FLuaActorProxy FLuaComponentProxy::GetOwner() const
{
	FLuaActorProxy OwnerProxy;

	UActorComponent* TargetComponent = GetComponent();
	if (!TargetComponent)
	{
		return OwnerProxy;
	}

	// Owner도 ActorProxy로 감싸서 반환한다.
	// 이렇게 해야 Lua가 Component에서 역으로 Actor를 얻더라도 AActor*에 직접 접근하지 못한다.
	OwnerProxy.Actor = ResolveAliveActor(TargetComponent->GetOwner());
	return OwnerProxy;
}

void FLuaComponentProxy::SetActive(bool bActive)
{
	UActorComponent* TargetComponent = GetComponent();
	if (!TargetComponent)
	{
		return;
	}

	// 실제 활성화 구현은 Component가 갖고 있으므로 Proxy는 생존 체크와 API 제한만 담당한다.
	TargetComponent->SetActive(bActive);
}

bool FLuaComponentProxy::IsActive() const
{
	UActorComponent* TargetComponent = GetComponent();
	return TargetComponent ? TargetComponent->IsActive() : false;
}

// ================================================================
// FLuaScriptRuntime Section
// - Lua System 전체 관리자
// ================================================================
FLuaScriptRuntime::FLuaScriptRuntime()
{
	Initialize();
}

FLuaScriptRuntime::~FLuaScriptRuntime()
{
	Shutdown();
}

bool FLuaScriptRuntime::Initialize()
{
	if (bInitialized)
	{
		return true;
	}

	LastError.clear();

	try
	{
		// Runtime은 Lua VM 하나를 전역으로 유지하고,
		// 각 ScriptInstance는 이 VM 위에 독립된 environment를 만들어 쓴다.
		Impl = std::make_unique<FRuntimeImpl>();
		Impl->Lua.open_libraries(
			sol::lib::base,
			sol::lib::math,
			sol::lib::string,
			sol::lib::table,
			sol::lib::coroutine
			/*sol::lib::package*/); // 아직 require을 지원하지 않는다
		// TODO: 필요하면 그때 추가

		RegisterBindings();

		// 타입 바인딩이 끝난 뒤 watcher를 붙여야
		// 핫 리로드가 들어와도 재로드 경로가 즉시 동작할 수 있다.
		InitializeHotReload();

		bInitialized = true;
		return true;
	}
	catch (const std::exception& Exception)
	{
		ShutdownHotReload();
		LastError = Exception.what();
		bInitialized = false;
		Impl.reset();
		UE_LOG("[LuaRuntime] Initialize failed: %s", LastError.c_str());
		return false;
	}
}

void FLuaScriptRuntime::Shutdown()
{
	// watcher를 먼저 정리해서 shutdown 이후 콜백이 날아오지 않게 막는다.
	ShutdownHotReload();
	Impl.reset();
	bInitialized = false;
	LastError.clear();
}

sol::state& FLuaScriptRuntime::GetLuaState()
{
	if (!Impl)
	{
		// 정상 경로에서는 Initialize가 먼저 호출되지만,
		// 방어적으로 접근 시점에 Impl이 비어 있으면 최소한의 저장소는 다시 만든다.
		Impl = std::make_unique<FRuntimeImpl>();
	}

	return Impl->Lua;
}

const FString& FLuaScriptRuntime::GetLastError() const
{
	return LastError;
}

void FLuaScriptRuntime::RegisterScriptComponent(UScriptComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// 같은 컴포넌트가 여러 번 등록돼도 TSet이 중복을 막아준다.
	ScriptComponents.insert(Component);
}

void FLuaScriptRuntime::UnregisterScriptComponent(UScriptComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// EndPlay나 경로 변경 등 어떤 이유든 더 이상 대상이 아니면 즉시 목록에서 뺀다.
	ScriptComponents.erase(Component);
}

void FLuaScriptRuntime::RegisterBindings()
{
	// 런타임 레벨에서 공통으로 노출할 타입은 모두 여기서 한 번만 등록한다.
	BindVectorType();
	BindActorProxyType();
	// ComponentProxy/Color 바인딩은 유지 보수용 함수만 남기고 공개하지 않는다.
	// TODO: 해당 기능이 실제 스크립트 요구사항으로 돌아오면 EngineAPI.lua와 함께 다시 노출한다.
}

void FLuaScriptRuntime::InitializeHotReload()
{
	if (WatchSub != 0)
	{
		return;
	}

	// watcher가 넘겨주는 경로도 내부 저장 경로와 같은 "Scripts/" prefix를 쓰게 해서
	// 이후 비교 단계에서 별도 remap 없이 바로 ScriptPath와 대조할 수 있게 만든다.
	const FWatchID WatchID = FDirectoryWatcher::Get().Watch(FPaths::ScriptsDir(), "Scripts/");
	if (WatchID == 0)
	{
		UE_LOG("[LuaRuntime] Failed to watch Scripts directory.");
		return;
	}

	WatchSub = FDirectoryWatcher::Get().Subscribe(
		WatchID,
		[this](const TSet<FString>& Files)
		{
			// ProcessChanges()에서 메인 스레드로 디스패치되므로
			// 여기서 바로 UObject 접근과 ReloadScript() 호출이 가능하다.
			OnScriptsChanged(Files);
		});
}

void FLuaScriptRuntime::ShutdownHotReload()
{
	if (WatchSub != 0)
	{
		FDirectoryWatcher::Get().Unsubscribe(WatchSub);
		WatchSub = 0;
	}

	// 플레이 세션이 끝나면 이전 component 포인터를 다음 세션까지 끌고 가지 않는다.
	ScriptComponents.clear();
}

void FLuaScriptRuntime::OnScriptsChanged(const TSet<FString>& ChangedFiles)
{
	TSet<FString> ChangedLuaScripts;
	for (const FString& ChangedFile : ChangedFiles)
	{
		if (!IsLuaScriptFile(ChangedFile))
		{
			continue;
		}

		const FString NormalizedPath = FScriptPaths::NormalizeScriptPath(ChangedFile);
		if (!NormalizedPath.empty())
		{
			// Watch prefix도 Scripts/ 이고 내부 저장 경로도 Scripts/ 로 통일돼 있으므로
			// 여기서 한 번만 정규화하면 곧바로 component ScriptPath와 비교할 수 있다.
			ChangedLuaScripts.insert(NormalizedPath);
		}
	}

	if (ChangedLuaScripts.empty() || ScriptComponents.empty())
	{
		return;
	}

	// 순회 중 죽은 UObject를 바로 erase하면 iterator 관리가 번거로워지므로
	// stale 목록에 모아 뒀다가 마지막에 한 번에 정리한다.
	TArray<UScriptComponent*> StaleComponents;
	for (UScriptComponent* Component : ScriptComponents)
	{
		if (!Component || !IsAliveObject(Component))
		{
			StaleComponents.push_back(Component);
			continue;
		}

		const FString ComponentScriptPath = FScriptPaths::NormalizeScriptPath(Component->GetScriptPath());
		if (ComponentScriptPath.empty() || ChangedLuaScripts.find(ComponentScriptPath) == ChangedLuaScripts.end())
		{
			continue;
		}

		// 실제 재초기화 순서는 Component가 알고 있으므로 Runtime은 위임만 한다.
		if (Component->ReloadScript())
		{
			UE_LOG("[ScriptHotReload] Reloaded: %s", ComponentScriptPath.c_str());
			FNotificationManager::Get().AddNotification("Script Reloaded: " + ComponentScriptPath, ENotificationType::Success, 3.0f);
		}
		else
		{
			const FString ReloadError = Component->GetLastScriptError();
			if (ReloadError.empty())
			{
				UE_LOG("[ScriptHotReload] Failed: %s", ComponentScriptPath.c_str());
			}
			else
			{
				UE_LOG("[ScriptHotReload] Failed: %s (%s)", ComponentScriptPath.c_str(), ReloadError.c_str());
			}
			FNotificationManager::Get().AddNotification("Script Failed: " + ComponentScriptPath, ENotificationType::Error, 5.0f);
		}
	}

	for (UScriptComponent* StaleComponent : StaleComponents)
	{
		// UObject가 이미 파괴된 포인터는 다음 이벤트부터 순회하지 않도록 정리한다.
		ScriptComponents.erase(StaleComponent);
	}
}
// TODO: 어쩔 수 없이 하드코딩으로 바인딩 -> 추후 구조 개선 고려
void FLuaScriptRuntime::BindVectorType()
{
	sol::state& Lua = GetLuaState();

	// 스크립트에서 수학 타입을 자연스럽게 쓰도록 연산자까지 함께 바인딩한다.
	Lua.new_usertype<FVector>(
	"Vector",
	sol::constructors<FVector(), FVector(float, float, float)>(),
	"x", sol::property([](const FVector& Value) { return Value.X; }, [](FVector& Value, float InX) { Value.X = InX; }),
	"y", sol::property([](const FVector& Value) { return Value.Y; }, [](FVector& Value, float InY) { Value.Y = InY; }),
	"z", sol::property([](const FVector& Value) { return Value.Z; }, [](FVector& Value, float InZ) { Value.Z = InZ; }),
	sol::meta_function::addition, [](const FVector& A, const FVector& B) { return A + B; },
	sol::meta_function::subtraction, [](const FVector& A, const FVector& B) { return A - B; },
	sol::meta_function::multiplication, sol::overload(
		[](const FVector& A, float Scalar) { return A * Scalar; },
		[](float Scalar, const FVector& A) { return A * Scalar; }),
	sol::meta_function::division, [](const FVector& A, float Scalar)
	{
		return Scalar == 0.0f ? FVector(0.0f, 0.0f, 0.0f) : (A / Scalar);
	});

	// Vector Generator
	Lua.set_function("vec3", [](float X, float Y, float Z)
	{
		return FVector(X, Y, Z);
	});

	// Print - sol library base에서 지원할 수 있지만, winMain 환경에서 안될 수 도 있음
	Lua.set_function("print", [](sol::variadic_args Args)
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

			UE_LOG("[Lua] %s", Message.c_str());
		});

	// C++에서 함수 호출 예시
	// Lua["print"]("Hello Lua!");
}

void FLuaScriptRuntime::BindComponentProxyType()
{
	sol::state& Lua = GetLuaState();

	// ComponentProxy는 UActorComponent*를 Lua에 직접 공개하지 않기 위한 얇은 접근 계층이다.
	// 각 함수 내부에서 Component와 Owner Actor 생존 여부를 다시 확인하므로, Lua가 Proxy 값을 오래 들고 있어도 안전하게 실패한다.
	Lua.new_usertype<FLuaComponentProxy>(
		"ComponentProxy",
		"IsValid", &FLuaComponentProxy::IsValid,
		"Name", sol::property(&FLuaComponentProxy::GetName),
		"Owner", sol::property(&FLuaComponentProxy::GetOwner),
		"SetActive", &FLuaComponentProxy::SetActive,
		"IsActive", &FLuaComponentProxy::IsActive);
}

void FLuaScriptRuntime::BindActorProxyType()
{
	sol::state& Lua = GetLuaState();

	// 실제 Actor 전체를 노출하지 않고, 스크립트에 허용한 조작만 Proxy에 제한해서 공개한다.
	// 아래 목록이 EngineAPI.lua의 Actor stub과 맞아야 LuaLS 자동완성과 실제 런타임 동작이 어긋나지 않는다.
	Lua.new_usertype<FLuaActorProxy>(
		"ActorProxy",
		"IsValid", &FLuaActorProxy::IsValid,
		"Name", sol::property(&FLuaActorProxy::GetName),
		"UUID", sol::property(&FLuaActorProxy::GetUUID),
		"Location", sol::property(&FLuaActorProxy::GetLocation, &FLuaActorProxy::SetLocation),
		"Rotation", sol::property(&FLuaActorProxy::GetRotation, &FLuaActorProxy::SetRotation),
		"Scale", sol::property(&FLuaActorProxy::GetScale, &FLuaActorProxy::SetScale),
		"Velocity", sol::property(&FLuaActorProxy::GetVelocity, &FLuaActorProxy::SetVelocity),
		"AddWorldOffset", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::AddWorldOffset),
			&FLuaActorProxy::AddWorldOffsetXYZ),
		"MoveTo", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::MoveTo),
			&FLuaActorProxy::MoveTo2D,
			&FLuaActorProxy::MoveTo3D),
		"MoveBy", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::MoveBy),
			&FLuaActorProxy::MoveBy2D,
			&FLuaActorProxy::MoveBy3D),
		"MoveToActor", &FLuaActorProxy::MoveToActor,
		"StopMove", &FLuaActorProxy::StopMove,
		"IsMoveDone", &FLuaActorProxy::IsMoveDone,
		"SetMoveSpeed", &FLuaActorProxy::SetMoveSpeed,
		"GetMoveSpeed", &FLuaActorProxy::GetMoveSpeed,
		"PrintLocation", &FLuaActorProxy::PrintLocation);
}

void FLuaScriptRuntime::BindColorType()
{
	sol::state& Lua = GetLuaState();

	Lua.new_usertype<FColor>(
		"Color",
		sol::constructors<FColor(), FColor(int32, int32, int32, int32)>(),
		"r", sol::property([](const FColor& Value) { return Value.R; }, [](FColor& Value, int32 InR) { Value.R = InR; }),
		"g", sol::property([](const FColor& Value) { return Value.G; }, [](FColor& Value, int32 InG) { Value.G = InG; }),
		"b", sol::property([](const FColor& Value) { return Value.B; }, [](FColor& Value, int32 InB) { Value.B = InB; }),
		"a", sol::property([](const FColor& Value) { return Value.A; }, [](FColor& Value, int32 InA) { Value.A = InA; })
	);

	Lua.set_function("MakeColor", [](int32 R, int32 G, int32 B, int32 A)
		{
			return FColor(R, G, B, A);
		});
}
