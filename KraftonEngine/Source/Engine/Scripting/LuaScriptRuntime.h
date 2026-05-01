#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

#include <memory>
#include "Core/Singleton.h"

class AActor;
class UActorComponent;
class UScriptComponent;
struct FLuaComponentProxy;

namespace sol
{
	class state;
}
// =================================================
// Lua에 공개할 기능을 제한하는 FLuaActorProxy
// - Lua Script 작성자가 엔진 내부 규칙을 몰라도 사용 가능하게 한다
// - Lua에서 Actor정보에 바로 접근하지 않고, Proxy를 통해서 거치게 한다
// - Proxy를 통해 함수 커스텀 가능
// - 너무 비대해질 경우, 상속을 통한 여러 type 두기 고려
// =================================================
struct FLuaActorProxy
{
	// Proxy는 AActor를 소유하지 않는다.
	// Lua에 넘어간 값이 GC되거나 복사되어도 실제 Actor 생명주기는 World/Object system이 관리해야 하므로,
	// 여기에는 약한 참조처럼 사용할 raw pointer와 Proxy끼리 공유하는 스크립트 작업 상태만 둔다.
	struct FLuaActorTaskState;

	FLuaActorProxy();

	AActor* Actor = nullptr;
	std::shared_ptr<FLuaActorTaskState> TaskState;
	FVector Velocity = FVector(1.0f, 0.0f, 0.0f);

	// Lua가 받은 Proxy가 아직 살아 있는 Actor를 가리키는지 확인한다.
	bool IsValid() const;

	// 모든 Proxy 함수는 실제 작업 전에 이 함수를 거쳐 죽은 Actor 접근을 차단한다.
	AActor* GetActor() const;

	// Actor 이름/ID 조회도 생존 체크 후 안전하게 실패한다.
	FString GetName() const;
	uint32 GetUUID() const;

	// Transform API는 Lua가 Actor 내부 포인터를 직접 만지지 않고 Proxy를 통해서만 위치를 조작하게 만든다.
	FVector GetLocation() const;
	void SetLocation(const FVector& InLocation);

	FVector GetRotation() const;
	void SetRotation(const FVector& InRotation);

	FVector GetScale() const;
	void SetScale(const FVector& InScale);

	FVector GetVelocity() const;
	void SetVelocity(const FVector& InVelocity);

	void AddWorldOffset(const FVector& Delta);
	void AddWorldOffsetXYZ(float X, float Y, float Z);

	// 이동 API는 즉시 위치를 바꾸는 함수가 아니라 C++ Tick에서 처리할 목표 상태를 설정한다.
	void MoveTo(const FVector& Target);
	void MoveTo2D(float X, float Y);
	void MoveTo3D(float X, float Y, float Z);

	void MoveBy(const FVector& Delta);
	void MoveBy2D(float X, float Y);
	void MoveBy3D(float X, float Y, float Z);

	void MoveToActor(const FLuaActorProxy& TargetActor);

	void StopMove();
	bool IsMoveDone() const;
	void SetMoveSpeed(float InSpeed);
	float GetMoveSpeed() const;

	// 실제 애니메이션 시스템이 붙기 전까지 코루틴 대기 조건을 검증하기 위한 1초 mock API이다.
	void PlayAnimation(const FString& AnimName);
	bool IsAnimationDone(const FString& AnimName) const;

	// Actor가 가진 Component를 Lua에 직접 포인터로 넘기지 않고, 항상 ComponentProxy로 감싸서 반환한다.
	// 이름 조회는 실제 UObject 이름과 Component class 이름을 모두 허용해서 스크립트 사용성을 높인다.
	FLuaComponentProxy GetComponent(const FString& ComponentName);
	// class 이름으로 Component를 찾는다. "UStaticMeshComponent"와 "StaticMeshComponent" 표기를 모두 허용한다.
	FLuaComponentProxy GetComponentByType(const FString& TypeName);
	// 자주 쓰는 Component는 문자열 하드코딩 없이 호출할 수 있게 편의 함수를 둔다.
	FLuaComponentProxy GetScriptComponent();
	FLuaComponentProxy GetStaticMeshComponent();

	// Lua coroutine scheduler가 매 프레임 호출해서 Proxy 내부 비동기 작업을 진행한다.
	void TickLuaTasks(float DeltaTime);
	void TickMovement(float DeltaTime);
	void TickAnimationMock(float DeltaTime);

	void PrintLocation() const;
	void Destroy();
};

// =================================================
// Lua에 공개할 Component 접근용 FLuaComponentProxy
// - Lua에는 UActorComponent*를 직접 넘기지 않고 이 Proxy 값만 전달한다.
// - Proxy는 Component를 소유하지 않으며, ActorProxy와 동일하게 약한 참조처럼 동작한다.
// - 모든 public 함수는 내부에서 Component와 Owner Actor 생존 여부를 다시 확인한다.
// =================================================
struct FLuaComponentProxy
{
	// 실제 Component 수명은 Actor/Object system이 관리한다.
	// Lua 쪽 Proxy는 이 포인터를 직접 소유하거나 삭제하지 않고, 매 호출마다 생존 여부만 확인한다.
	UActorComponent* Component = nullptr;

	// 이 Proxy가 아직 살아 있는 Component와 살아 있는 Owner Actor를 가리키는지 확인한다.
	bool IsValid() const;

	// 모든 ComponentProxy 함수는 실제 Component 접근 전에 이 안전 조회를 통과한다.
	UActorComponent* GetComponent() const;

	// Component 이름을 반환한다. 유효하지 않으면 빈 문자열을 반환한다.
	FString GetName() const;

	// Component의 Owner Actor를 ActorProxy로 반환한다. 유효하지 않으면 빈 ActorProxy를 반환한다.
	FLuaActorProxy GetOwner() const;

	// Component 활성화 상태를 변경한다. Component나 Owner Actor가 죽었으면 아무 일도 하지 않는다.
	void SetActive(bool bActive);

	// Component 활성화 상태를 반환한다. 유효하지 않으면 false를 반환한다.
	bool IsActive() const;
};

// =================================================================
// Lua 의 Runtime 수명 주기를 관리하는 Manager 클래스
// =================================================================
class FLuaScriptRuntime : public TSingleton<FLuaScriptRuntime>
{
	friend class TSingleton<FLuaScriptRuntime>;
public:
	FLuaScriptRuntime();
	~FLuaScriptRuntime();

	// 전역 Lua VM, 기본 타입 바인딩, Scripts watcher를 준비한다
	bool Initialize();

	// watcher 구독과 등록된 component 목록, Lua VM을 모두 정리한다
	void Shutdown();

	// 모든 ScriptInstance가 공유하는 전역 Lua 상태를 반환한다
	sol::state& GetLuaState();

	const FString& GetLastError() const;

	// 플레이 중인 ScriptComponent만 등록 대상으로 관리한다
	// 실제 파일 변경 감지는 Runtime이 전역으로 담당하고,
	// 어떤 인스턴스를 다시 로드할지는 이 목록을 기준으로 결정한다
	void RegisterScriptComponent(UScriptComponent* Component);
	void UnregisterScriptComponent(UScriptComponent* Component);

	bool bIsInitialized() const { return bInitialized; }

private:
	// Lua에 노출할 공용 타입/함수를 한 곳에서 등록한다.
	void RegisterBindings();

	void BindVectorType();		// Vector
	void BindComponentProxyType(); // ComponentProxy
	void BindActorProxyType();	// ActorProxy
	void BindColorType();		// Color

	// Scripts/ 폴더 watcher를 붙였다가 떼는 수명 관리 함수
	void InitializeHotReload();
	void ShutdownHotReload();

	// DirectoryWatcher가 모아준 변경 파일 집합을 받아
	// 해당 스크립트를 사용하는 컴포넌트만 선별해서 ReloadScript()를 호출한다.
	void OnScriptsChanged(const TSet<FString>& ChangedFiles);

private:
	struct FRuntimeImpl;
	std::unique_ptr<FRuntimeImpl> Impl;
	bool bInitialized = false;
	FString LastError;
	uint32 WatchSub = 0;

	// UObject 수명은 외부에서 관리하므로 소유하지 않는다.
	// 죽은 객체는 hot-reload 처리 중 정리한다.
	TSet<UScriptComponent*> ScriptComponents;
};
