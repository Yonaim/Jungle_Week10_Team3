#include "Scripting/LuaScriptRuntime.h"

#include "Component/ScriptComponent.h"
#include "Core/EngineTypes.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "Object/Object.h"
#include "Platform/DirectoryWatcher.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"

// 이거 넣는 이유: sol/sol.hpp 내장 check하고 충돌 방지 목적
#pragma region SolInclude
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
#include <filesystem>

struct FLuaScriptRuntime::FRuntimeImpl
{
	// 런타임이 소유하는 전역 Lua VM.
	// 개별 ScriptInstance는 이 VM 위에 environment만 따로 만든다.
	sol::state Lua;
};

namespace
{
	void ConfigurePackagePath(sol::state& Lua)
	{
		sol::table Package = Lua["package"];
		if (!Package.valid())
		{
			return;
		}

		std::filesystem::path ScriptRootPath(FPaths::ScriptsDir());
		FString ScriptRoot = FPaths::ToUtf8(ScriptRootPath.lexically_normal().generic_wstring());
		while (!ScriptRoot.empty() && (ScriptRoot.back() == '/' || ScriptRoot.back() == '\\'))
		{
			ScriptRoot.pop_back();
		}

		// require는 프로젝트 Scripts 폴더 아래만 찾게 제한한다.
		// 기존 package.path를 뒤에 붙이면 실행 환경마다 외부 Lua 파일이 섞일 수 있으므로 사용하지 않는다.
		Package["path"] = ScriptRoot + "/?.lua;" + ScriptRoot + "/?/init.lua";
		Package["cpath"] = "";

		// require로 불러온 모듈은 Lua state 안에서 캐시됩니다.
		// 여러 스크립트가 같은 모듈 테이블을 공유하므로 Config는 상수처럼 사용하는 것이 안전합니다.
	}

	bool IsLuaScriptFile(const FString& Path)
	{
		// DirectoryWatcher는 prefix 포함 상대 경로를 넘겨주므로 확장자만 보고 Lua 대상인지 빠르게 걸러낸다.
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
			sol::lib::coroutine,
			sol::lib::package);
		ConfigurePackagePath(Impl->Lua);

		RegisterBindings();

		// 타입 바인딩이 끝난 뒤 watcher를 붙여야 핫 리로드가 즉시 동작할 수 있다.
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
		UE_LOG_CATEGORY(LuaRuntime, Error, "Initialize failed: %s", LastError.c_str());
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
		// 정상 경로에서는 Initialize가 먼저 호출되지만, 방어적으로 접근 시점에 저장소를 다시 만든다.
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
	BindComponentProxyType();
	// Color 바인딩은 유지 보수용 함수만 남기고 공개하지 않는다.
}

void FLuaScriptRuntime::InitializeHotReload()
{
	if (WatchSub != 0)
	{
		return;
	}

	// watcher가 넘겨주는 경로도 내부 저장 경로와 같은 "Scripts/" prefix를 쓰게 한다.
	const FWatchID WatchID = FDirectoryWatcher::Get().Watch(FPaths::ScriptsDir(), "Scripts/");
	if (WatchID == 0)
	{
		UE_LOG_CATEGORY(LuaRuntime, Warning, "Failed to watch Scripts directory.");
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
			UE_LOG_CATEGORY(ScriptHotReload, Info, "Reloaded: %s", ComponentScriptPath.c_str());
			FNotificationManager::Get().AddNotification("Script Reloaded: " + ComponentScriptPath, ENotificationType::Success, 3.0f);
		}
		else
		{
			const FString ReloadError = Component->GetLastScriptError();
			if (ReloadError.empty())
			{
				UE_LOG_CATEGORY(ScriptHotReload, Error, "Failed: %s", ComponentScriptPath.c_str());
			}
			else
			{
				UE_LOG_CATEGORY(ScriptHotReload, Error, "Failed: %s (%s)", ComponentScriptPath.c_str(), ReloadError.c_str());
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

	Lua.set_function("vec3", [](float X, float Y, float Z)
	{
		return FVector(X, Y, Z);
	});

	Lua.set_function("Vector3", [](float X, float Y, float Z)
	{
		return FVector(X, Y, Z);
	});

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
}

void FLuaScriptRuntime::BindComponentProxyType()
{
	sol::state& Lua = GetLuaState();

	// ComponentProxy는 UActorComponent*를 Lua에 직접 공개하지 않기 위한 얇은 접근 계층이다.
	Lua.new_usertype<FLuaComponentProxy>(
		"ComponentProxy",
		"IsValid", &FLuaComponentProxy::IsValid,
		"Name", sol::property(&FLuaComponentProxy::GetName),
		"Owner", sol::property(&FLuaComponentProxy::GetOwner),
		"TypeName", sol::property(&FLuaComponentProxy::GetTypeName),
		"GetTypeName", &FLuaComponentProxy::GetTypeName,
		"SetActive", &FLuaComponentProxy::SetActive,
		"IsActive", &FLuaComponentProxy::IsActive,
		"GetLocation", &FLuaComponentProxy::GetLocation,
		"SetLocation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetLocation),
			&FLuaComponentProxy::SetLocationXYZ),
		"AddWorldOffset", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::AddWorldOffset),
			&FLuaComponentProxy::AddWorldOffsetXYZ),
		"Translate", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::AddWorldOffset),
			&FLuaComponentProxy::AddWorldOffsetXYZ),
		"GetRotation", &FLuaComponentProxy::GetRotation,
		"SetRotation", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetRotation),
			&FLuaComponentProxy::SetRotationXYZ),
		"GetScale", &FLuaComponentProxy::GetScale,
		"SetScale", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::SetScale),
			&FLuaComponentProxy::SetScaleXYZ),
		"SetCollisionEnabled", &FLuaComponentProxy::SetCollisionEnabled,
		"SetGenerateOverlapEvents", &FLuaComponentProxy::SetGenerateOverlapEvents,
		"IsOverlappingActor", &FLuaComponentProxy::IsOverlappingActor,
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
		"SetSoundPath", &FLuaComponentProxy::SetSoundPath,
		"GetSoundPath", &FLuaComponentProxy::GetSoundPath,
		"SetSoundCategory", &FLuaComponentProxy::SetSoundCategory,
		"GetSoundCategory", &FLuaComponentProxy::GetSoundCategory,
		"SetSoundLooping", &FLuaComponentProxy::SetSoundLooping,
		"IsSoundLooping", &FLuaComponentProxy::IsSoundLooping,
		"PlaySound", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)()>(&FLuaComponentProxy::PlayAudio),
			&FLuaComponentProxy::PlayAudioPath),
		"StopSound", &FLuaComponentProxy::StopSound,
		"PauseSound", &FLuaComponentProxy::PauseSound,
		"ResumeSound", &FLuaComponentProxy::ResumeSound,
		"IsSoundPlaying", &FLuaComponentProxy::IsSoundPlaying,
		"SetSpeed", &FLuaComponentProxy::SetSpeed,
		"GetSpeed", &FLuaComponentProxy::GetSpeed,
		"MoveTo", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::MoveTo),
			&FLuaComponentProxy::MoveToXYZ),
		"MoveBy", sol::overload(
			static_cast<bool(FLuaComponentProxy::*)(const FVector&)>(&FLuaComponentProxy::MoveBy),
			&FLuaComponentProxy::MoveByXYZ),
		"StopMove", &FLuaComponentProxy::StopMove,
		"IsMoveDone", &FLuaComponentProxy::IsMoveDone);
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
		"Tag", sol::property(&FLuaActorProxy::GetTag, &FLuaActorProxy::SetTag),
		"Location", sol::property(&FLuaActorProxy::GetLocation, &FLuaActorProxy::SetLocation),
		"Rotation", sol::property(&FLuaActorProxy::GetRotation, &FLuaActorProxy::SetRotation),
		"Scale", sol::property(&FLuaActorProxy::GetScale, &FLuaActorProxy::SetScale),
		"Velocity", sol::property(&FLuaActorProxy::GetVelocity, &FLuaActorProxy::SetVelocity),

		"HasTag", &FLuaActorProxy::HasTag,
		"GetComponent", &FLuaActorProxy::GetComponent,
		"GetComponentByType", &FLuaActorProxy::GetComponentByType,
		"GetScriptComponent", &FLuaActorProxy::GetScriptComponent,
		"GetStaticMeshComponent", &FLuaActorProxy::GetStaticMeshComponent,

		"AddWorldOffset", sol::overload(
			static_cast<void(FLuaActorProxy::*)(const FVector&)>(&FLuaActorProxy::AddWorldOffset),
			&FLuaActorProxy::AddWorldOffsetXYZ),
		"Translate", sol::overload(
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
		"PrintLocation", &FLuaActorProxy::PrintLocation,
		"Destroy", &FLuaActorProxy::Destroy);
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
