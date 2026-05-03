#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"

class UActorComponent;
struct FLuaActorProxy;

namespace sol
{
	template <typename T>
	class optional;
}

// =================================================
// Lua에 공개할 Component 접근용 FLuaComponentProxy
// - Lua에는 UActorComponent*를 직접 넘기지 않고 이 Proxy 값만 전달한다.
// - 모든 public 함수는 내부에서 Component와 Owner Actor 생존 여부를 다시 확인한다.
// =================================================
struct FLuaComponentProxy
{
	// 실제 Component 수명은 Actor/Object system이 관리한다.
	UActorComponent* Component = nullptr;

	bool IsValid() const;
	UActorComponent* GetComponent() const;

	FString GetName() const;
	FString GetTypeName() const;
	FLuaActorProxy GetOwner() const;

	bool SetActive(bool bActive);
	bool IsActive() const;

	// SceneComponent 계열 transform. 지원하지 않는 Component에서는 nil/false를 반환한다.
	sol::optional<FVector> GetLocation() const;
	bool SetLocation(const FVector& InLocation);
	bool SetLocationXYZ(float X, float Y, float Z);
	bool AddWorldOffset(const FVector& Delta);
	bool AddWorldOffsetXYZ(float X, float Y, float Z);
	sol::optional<FVector> GetRotation() const;
	bool SetRotation(const FVector& InRotation);
	bool SetRotationXYZ(float X, float Y, float Z);
	sol::optional<FVector> GetScale() const;
	bool SetScale(const FVector& InScale);
	bool SetScaleXYZ(float X, float Y, float Z);

	// Primitive/Shape collision. 지원하지 않는 Component에서는 false를 반환한다.
	bool SetCollisionEnabled(bool bEnabled);
	bool SetGenerateOverlapEvents(bool bEnabled);
	bool IsOverlappingActor(const FLuaActorProxy& OtherActor) const;

	// Lua에서 테스트용 Mesh를 빠르게 바꾸기 위한 함수입니다.
	// 장기적으로는 Asset ID 기반 로딩으로 교체하는 것이 안전합니다.
	bool SetStaticMesh(const FString& MeshPath);

	// TextRenderComponent. 지원하지 않는 Component에서는 false/nil을 반환한다.
	bool SetText(const FString& Text);
	sol::optional<FString> GetText() const;

	// Movement 계열. 현재 존재하는 InterpTo/Projectile movement에만 얇게 연결한다.
	bool SetSpeed(float Speed);
	sol::optional<float> GetSpeed() const;
	bool MoveTo(const FVector& Target);
	bool MoveToXYZ(float X, float Y, float Z);
	bool MoveBy(const FVector& Delta);
	bool MoveByXYZ(float X, float Y, float Z);
	bool StopMove();
	bool IsMoveDone() const;
};
