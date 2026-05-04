#include "Scripting/LuaScriptRuntime.h"

#ifdef check
#pragma push_macro("check")
#undef check
#define LUA_PROXY_BINDINGS_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define LUA_PROXY_BINDINGS_RESTORE_CHECKF_MACRO
#endif

#include <sol/sol.hpp>

#ifdef LUA_PROXY_BINDINGS_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef LUA_PROXY_BINDINGS_RESTORE_CHECKF_MACRO
#endif

#ifdef LUA_PROXY_BINDINGS_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef LUA_PROXY_BINDINGS_RESTORE_CHECK_MACRO
#endif

void FLuaScriptRuntime::BindComponentProxyType()
{
	sol::state& Lua = GetLuaState();

	Lua.new_usertype<FLuaComponentProxy>(
		"ComponentProxy",
		"IsValid", &FLuaComponentProxy::IsValid,
		"Name", sol::property(&FLuaComponentProxy::GetName),
		"Owner", sol::property(&FLuaComponentProxy::GetOwner),
		"TypeName", sol::property(&FLuaComponentProxy::GetTypeName),
		"GetTypeName", &FLuaComponentProxy::GetTypeName,
		"SetActive", &FLuaComponentProxy::SetActive,
		"IsActive", &FLuaComponentProxy::IsActive,
		"SetVisible", &FLuaComponentProxy::SetVisible,
		"IsVisible", &FLuaComponentProxy::IsVisible,
		"GetWorldLocation", &FLuaComponentProxy::GetWorldLocation,
		"SetWorldLocation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetWorldLocation),
			&FLuaComponentProxy::SetWorldLocationXYZ),
		"SetWorldLocationXYZ", &FLuaComponentProxy::SetWorldLocationXYZ,
		"GetLocalLocation", &FLuaComponentProxy::GetLocalLocation,
		"SetLocalLocation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetLocalLocation),
			&FLuaComponentProxy::SetLocalLocationXYZ),
		"SetLocalLocationXYZ", &FLuaComponentProxy::SetLocalLocationXYZ,
		"AddWorldOffset", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::AddWorldOffset),
			&FLuaComponentProxy::AddWorldOffsetXYZ),
		"AddWorldOffsetXYZ", &FLuaComponentProxy::AddWorldOffsetXYZ,
		"AddLocalOffset", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::AddLocalOffset),
			&FLuaComponentProxy::AddLocalOffsetXYZ),
		"AddLocalOffsetXYZ", &FLuaComponentProxy::AddLocalOffsetXYZ,
		"GetWorldRotation", &FLuaComponentProxy::GetWorldRotation,
		"SetWorldRotation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetWorldRotation),
			&FLuaComponentProxy::SetWorldRotationXYZ),
		"SetWorldRotationXYZ", &FLuaComponentProxy::SetWorldRotationXYZ,
		"GetLocalRotation", &FLuaComponentProxy::GetLocalRotation,
		"SetLocalRotation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetLocalRotation),
			&FLuaComponentProxy::SetLocalRotationXYZ),
		"SetLocalRotationXYZ", &FLuaComponentProxy::SetLocalRotationXYZ,
		"GetWorldScale", &FLuaComponentProxy::GetWorldScale,
		"SetWorldScale", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetWorldScale),
			&FLuaComponentProxy::SetWorldScaleXYZ),
		"SetWorldScaleXYZ", &FLuaComponentProxy::SetWorldScaleXYZ,
		"GetLocalScale", &FLuaComponentProxy::GetLocalScale,
		"SetLocalScale", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetLocalScale),
			&FLuaComponentProxy::SetLocalScaleXYZ),
		"SetLocalScaleXYZ", &FLuaComponentProxy::SetLocalScaleXYZ,
		"GetForwardVector", &FLuaComponentProxy::GetForwardVector,
		"GetRightVector", &FLuaComponentProxy::GetRightVector,
		"GetUpVector", &FLuaComponentProxy::GetUpVector,
		"SetCollisionEnabled", &FLuaComponentProxy::SetCollisionEnabled,
		"SetGenerateOverlapEvents", &FLuaComponentProxy::SetGenerateOverlapEvents,
		"IsOverlappingActor", &FLuaComponentProxy::IsOverlappingActor,
		"GetShapeType", &FLuaComponentProxy::GetShapeType,
		"GetShapeHalfHeight", &FLuaComponentProxy::GetShapeHalfHeight,
		"SetShapeHalfHeight", &FLuaComponentProxy::SetShapeHalfHeight,
		"GetShapeRadius", &FLuaComponentProxy::GetShapeRadius,
		"SetShapeRadius", &FLuaComponentProxy::SetShapeRadius,
		"GetShapeExtent", &FLuaComponentProxy::GetShapeExtent,
		"SetShapeExtent", &FLuaComponentProxy::SetShapeExtent,
		"SetStaticMesh", &FLuaComponentProxy::SetStaticMesh,
		"SetText", &FLuaComponentProxy::SetText,
		"GetText", &FLuaComponentProxy::GetText,
		"SetTexture", &FLuaComponentProxy::SetTexture,
		"GetTexturePath", &FLuaComponentProxy::GetTexturePath,
		"SetTint", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetTint),
			&FLuaComponentProxy::SetTintRGBA),
		"SetLabel", &FLuaComponentProxy::SetLabel,
		"GetLabel", &FLuaComponentProxy::GetLabel,
		"IsHovered", &FLuaComponentProxy::IsHovered,
		"IsPressed", &FLuaComponentProxy::IsPressed,
		"WasClicked", &FLuaComponentProxy::WasClicked,
		"SetAudioPath", &FLuaComponentProxy::SetAudioPath,
		"GetAudioPath", &FLuaComponentProxy::GetAudioPath,
		"SetAudioCategory", &FLuaComponentProxy::SetAudioCategory,
		"GetAudioCategory", &FLuaComponentProxy::GetAudioCategory,
		"SetAudioLooping", &FLuaComponentProxy::SetAudioLooping,
		"IsAudioLooping", &FLuaComponentProxy::IsAudioLooping,
		"PlayAudio", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)()>(&FLuaComponentProxy::PlayAudio),
			&FLuaComponentProxy::PlayAudioPath),
		"StopAudio", &FLuaComponentProxy::StopAudio,
		"PauseAudio", &FLuaComponentProxy::PauseAudio,
		"ResumeAudio", &FLuaComponentProxy::ResumeAudio,
		"IsAudioPlaying", &FLuaComponentProxy::IsAudioPlaying,
		"SetSpeed", &FLuaComponentProxy::SetSpeed,
		"GetSpeed", &FLuaComponentProxy::GetSpeed,
		"MoveTo", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::MoveTo),
			&FLuaComponentProxy::MoveToXYZ),
		"MoveBy", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::MoveBy),
			&FLuaComponentProxy::MoveByXYZ),
		"StopMove", &FLuaComponentProxy::StopMove,
		"IsMoveDone", &FLuaComponentProxy::IsMoveDone,
		"StartCameraShake", &FLuaComponentProxy::StartCameraShake,
		"AddHitEffect", &FLuaComponentProxy::AddHitEffect,
		"SetBoxExtent", sol::overload(
			&FLuaComponentProxy::SetBoxExtent,
			&FLuaComponentProxy::SetBoxExtentXYZ),
		"GetBoxExtent", &FLuaComponentProxy::GetBoxExtent);
}

void FLuaScriptRuntime::BindActorProxyType()
{
	sol::state& Lua = GetLuaState();

	Lua.new_usertype<FLuaActorProxy>(
		"ActorProxy",
		"IsValid", &FLuaActorProxy::IsValid,
		"Name", sol::property(&FLuaActorProxy::GetName),
		"UUID", sol::property(&FLuaActorProxy::GetUUID),
		"Tag", sol::property(&FLuaActorProxy::GetTag, &FLuaActorProxy::SetTag),
		"Location", sol::property(&FLuaActorProxy::GetWorldLocation, &FLuaActorProxy::SetWorldLocation),
		"Rotation", sol::property(&FLuaActorProxy::GetWorldRotation, &FLuaActorProxy::SetWorldRotation),
		"Scale", sol::property(&FLuaActorProxy::GetWorldScale, &FLuaActorProxy::SetWorldScale),
		"Velocity", sol::property(&FLuaActorProxy::GetVelocity, &FLuaActorProxy::SetVelocity),
		"HasTag", &FLuaActorProxy::HasTag,
		"GetWorldLocation", &FLuaActorProxy::GetWorldLocation,
		"SetWorldLocation", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::SetWorldLocation),
			&FLuaActorProxy::SetWorldLocationXYZ),
		"SetWorldLocationXYZ", &FLuaActorProxy::SetWorldLocationXYZ,
		"GetWorldRotation", &FLuaActorProxy::GetWorldRotation,
		"SetWorldRotation", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::SetWorldRotation),
			&FLuaActorProxy::SetWorldRotationXYZ),
		"SetWorldRotationXYZ", &FLuaActorProxy::SetWorldRotationXYZ,
		"GetWorldScale", &FLuaActorProxy::GetWorldScale,
		"SetWorldScale", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::SetWorldScale),
			&FLuaActorProxy::SetWorldScaleXYZ),
		"SetWorldScaleXYZ", &FLuaActorProxy::SetWorldScaleXYZ,
		"GetComponent", &FLuaActorProxy::GetComponent,
		"GetComponentByType", &FLuaActorProxy::GetComponentByType,
		"GetScriptComponent", &FLuaActorProxy::GetScriptComponent,
		"GetStaticMeshComponent", &FLuaActorProxy::GetStaticMeshComponent,
		"AddWorldOffset", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::AddWorldOffset),
			&FLuaActorProxy::AddWorldOffsetXYZ),
		"GetForwardVector", &FLuaActorProxy::GetForwardVector,
		"GetRightVector", &FLuaActorProxy::GetRightVector,
		"GetUpVector", &FLuaActorProxy::GetUpVector,
		"FindGround", [](const FLuaActorProxy& ActorProxy, float MaxDistance, float SkinWidth)
		{
			const FLuaGroundHit GroundHit = ActorProxy.FindGround(MaxDistance, SkinWidth);
			sol::table Result = FLuaScriptRuntime::Get().GetLuaState().create_table();
			Result["hit"] = GroundHit.bHit;
			Result["location"] = GroundHit.Location;
			Result["normal"] = GroundHit.Normal;
			Result["ground_z"] = GroundHit.GroundZ;
			Result["distance"] = GroundHit.Distance;
			Result["actor"] = GroundHit.Actor;
			Result["component"] = GroundHit.Component;
			return Result;
		},
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
		"PrintLocation", &FLuaActorProxy::PrintLocation,
		"Destroy", &FLuaActorProxy::Destroy);
}
