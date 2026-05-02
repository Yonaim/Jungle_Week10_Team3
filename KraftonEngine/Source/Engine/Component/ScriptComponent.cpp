#include "Component/ScriptComponent.h"

#include "Core/Log.h"
#include "Core/PropertyTypes.h"
#include "Editor/EditorEngine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Level.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"
#include "Runtime/Engine.h"
#include "Scripting/LuaScriptRuntime.h"
#include "Serialization/Archive.h"

#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

IMPLEMENT_CLASS(UScriptComponent, UActorComponent)

#pragma region Path&Property Helper
namespace
{
	// 스크립트 자동 생성 시 장면/액터 이름을 조합할 때 쓰는 fallback 이름.
	constexpr const char* DefaultSceneName = "DefaultScene";
	constexpr const char* DefaultActorName = "LuaActor";

	FString GetFileStem(const FString& PathString)
	{
		const std::filesystem::path Path(FPaths::ToWide(PathString));
		return FPaths::ToUtf8(Path.stem().wstring());
	}

	bool LooksLikeGeneratedSceneName(const FString& SceneName)
	{
		// 에디터나 런타임이 임시로 만든 이름은 실제 파일명 후보로 쓰지 않는다.
		return SceneName.empty()
			|| SceneName.rfind("ULevel_", 0) == 0
			|| SceneName.rfind("UWorld_", 0) == 0;
	}

	FString SanitizeScriptFileName(FString Name)
	{
		// 운영체제 파일명에 쓸 수 없는 문자만 최소한으로 치환한다.
		if (Name.empty())
		{
			return DefaultActorName;
		}

		for (char& Character : Name)
		{
			switch (Character)
			{
			case '<':
			case '>':
			case ':':
			case '"':
			case '/':
			case '\\':
			case '|':
			case '?':
			case '*':
			case ' ':
				Character = '_';
				break;
			default:
				break;
			}
		}

		return Name;
	}

	FString GetSceneNameForScript(const AActor* OwnerActor)
	{
		// 1순위는 실제 저장된 레벨 파일명이고,
		// 그게 없으면 Level/World 이름으로 fallback한다.
		if (const UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
		{
			if (EditorEngine->HasCurrentLevelFilePath())
			{
				const FString SceneStem = GetFileStem(EditorEngine->GetCurrentLevelFilePath());
				if (!SceneStem.empty())
				{
					return SceneStem;
				}
			}
		}

		if (OwnerActor)
		{
			if (const ULevel* Level = OwnerActor->GetLevel())
			{
				const FString LevelName = Level->GetFName().ToString();
				if (!LooksLikeGeneratedSceneName(LevelName))
				{
					return LevelName;
				}
			}

			if (const UWorld* World = OwnerActor->GetWorld())
			{
				const FString WorldName = World->GetFName().ToString();
				if (!LooksLikeGeneratedSceneName(WorldName))
				{
					return WorldName;
				}
			}
		}

		return DefaultSceneName;
	}

	FString GetActorNameForScript(const AActor* OwnerActor)
	{
		if (!OwnerActor)
		{
			return DefaultActorName;
		}

		const FString ActorName = OwnerActor->GetFName().ToString();
		return ActorName.empty() ? FString(DefaultActorName) : ActorName;
	}

	FString BuildDefaultScriptPath(const AActor* OwnerActor)
	{
		// 새 스크립트 생성 시 장면/액터 이름 기반 기본 파일명을 만들되,
		// 최종 표기는 항상 FScriptPaths 정책을 거쳐 Scripts/... 형태로 맞춘다.
		const FString SceneName = SanitizeScriptFileName(GetSceneNameForScript(OwnerActor));
		const FString ActorName = SanitizeScriptFileName(GetActorNameForScript(OwnerActor));
		const FString FileName = SceneName + "_" + ActorName + ".lua";
		return FScriptPaths::NormalizeScriptPath(FileName);
	}

	EPropertyType ToEditablePropertyType(EScriptPropertyType Type)
	{
		// Lua property 타입을 기존 Details 자동 위젯 타입으로 변환한다.
		// 에디터 렌더링 코드를 새로 만들지 않고 기존 PropertyDescriptor 파이프라인을 그대로 쓰기 위함이다.
		switch (Type)
		{
		case EScriptPropertyType::Bool:
			return EPropertyType::Bool;
		case EScriptPropertyType::Int:
			return EPropertyType::Int;
		case EScriptPropertyType::Float:
			return EPropertyType::Float;
		case EScriptPropertyType::String:
			return EPropertyType::String;
		case EScriptPropertyType::Vector:
			return EPropertyType::Vec3;
		default:
			return EPropertyType::Float;
		}
	}

	void* GetEditableScriptPropertyValuePtr(FScriptPropertyValue& Value)
	{
		// PropertyDescriptor는 void*를 요구하므로 실제 타입별 멤버 주소를 넘긴다.
		// 이 주소는 EditableScriptPropertyValues map 안의 값 수명에 묶여 있다.
		switch (Value.Type)
		{
		case EScriptPropertyType::Bool:
			return &Value.BoolValue;
		case EScriptPropertyType::Int:
			return &Value.IntValue;
		case EScriptPropertyType::Float:
			return &Value.FloatValue;
		case EScriptPropertyType::String:
			return &Value.StringValue;
		case EScriptPropertyType::Vector:
			return &Value.VectorValue;
		default:
			return &Value.FloatValue;
		}
	}

	FScriptPropertyValue CoerceScriptPropertyValue(const FScriptPropertyValue& Source, EScriptPropertyType TargetType, const FScriptPropertyValue& FallbackValue)
	{
		// Lua property("Speed", 600)처럼 fallback 타입과 선언 타입이 살짝 달라도
		// 숫자/불리언 정도는 자연스럽게 맞춰 준다. 문자열/벡터는 암묵 변환하지 않는다.
		if (Source.Type == TargetType)
		{
			return Source;
		}

		FScriptPropertyValue Result = FScriptProperty::MakeDefaultValue(TargetType);
		switch (TargetType)
		{
		case EScriptPropertyType::Bool:
			if (Source.Type == EScriptPropertyType::Int)
			{
				Result.BoolValue = Source.IntValue != 0;
			}
			else if (Source.Type == EScriptPropertyType::Float)
			{
				Result.BoolValue = Source.FloatValue != 0.0f;
			}
			else
			{
				Result.BoolValue = FallbackValue.BoolValue;
			}
			break;
		case EScriptPropertyType::Int:
			if (Source.Type == EScriptPropertyType::Bool)
			{
				Result.IntValue = Source.BoolValue ? 1 : 0;
			}
			else if (Source.Type == EScriptPropertyType::Float)
			{
				Result.IntValue = static_cast<int32>(Source.FloatValue);
			}
			else
			{
				Result.IntValue = FallbackValue.IntValue;
			}
			break;
		case EScriptPropertyType::Float:
			if (Source.Type == EScriptPropertyType::Bool)
			{
				Result.FloatValue = Source.BoolValue ? 1.0f : 0.0f;
			}
			else if (Source.Type == EScriptPropertyType::Int)
			{
				Result.FloatValue = static_cast<float>(Source.IntValue);
			}
			else
			{
				Result.FloatValue = FallbackValue.FloatValue;
			}
			break;
		case EScriptPropertyType::String:
			Result.StringValue = FallbackValue.StringValue;
			break;
		case EScriptPropertyType::Vector:
			Result.VectorValue = FallbackValue.VectorValue;
			break;
		default:
			break;
		}

		return Result;
	}

	void ClampScriptPropertyValue(FScriptPropertyValue& Value, const FScriptPropertyDesc& Desc)
	{
		// min/max는 숫자 타입 전용 힌트다. bool/string/vector에는 적용하지 않는다.
		if (Value.Type == EScriptPropertyType::Float)
		{
			if (Desc.bHasMin)
			{
				Value.FloatValue = (std::max)(Value.FloatValue, Desc.Min);
			}
			if (Desc.bHasMax)
			{
				Value.FloatValue = (std::min)(Value.FloatValue, Desc.Max);
			}
		}
		else if (Value.Type == EScriptPropertyType::Int)
		{
			if (Desc.bHasMin)
			{
				Value.IntValue = (std::max)(Value.IntValue, static_cast<int32>(Desc.Min));
			}
			if (Desc.bHasMax)
			{
				Value.IntValue = (std::min)(Value.IntValue, static_cast<int32>(Desc.Max));
			}
		}
	}
}
#pragma endregion

void UScriptComponent::BeginPlay()
{
	Super::BeginPlay();

	// 이전 플레이 세션에서 남았을 수 있는 component 수준 에러 상태를 먼저 비운다.
	ClearScriptError();

	if (ScriptPath.empty())
	{
		SetScriptError("ScriptPath is empty.");
		return;
	}

	// Play 중 hot-reload 대상 선별은 Runtime이 담당하므로
	// 실제 로딩 전에 먼저 등록해도 문제없다.
	UpdateRuntimeRegistration();

	if (!LoadScript())
	{
		return;
	}

	// 로드가 끝난 뒤 Lua BeginPlay를 호출해 스크립트 초기화 로직을 실행한다.
	// BeginPlay Call 실패 시 Error 출력 후 다시 load를 수행한다
	if (!ScriptInstance.CallBeginPlay())
	{
		RefreshScriptErrorState();

		ScriptInstance.StopAllCoroutines();
		bLoaded = false;

		return;
	}
	RefreshScriptErrorState();
}

void UScriptComponent::EndPlay()
{
	// EndPlay 이후에는 더 이상 이 component를 hot-reload 대상으로 순회하면 안 된다.
	if (!ScriptPath.empty())
	{
		FLuaScriptRuntime::Get().UnregisterScriptComponent(this);
	}

	if (bLoaded)
	{
		ScriptInstance.CallEndPlay();
	}

	ScriptInstance.StopAllCoroutines();
	ScriptInstance.Shutdown();
	bLoaded = false;
	RefreshScriptErrorState();

	Super::EndPlay();
}

void UScriptComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// ScriptPath는 component 상태의 일부이므로 저장/로드 시 그대로 직렬화한다.
	Ar << ScriptPath;

	uint32 OverrideCount = static_cast<uint32>(ScriptPropertyOverrides.size());
	Ar << OverrideCount;
	if (Ar.IsLoading())
	{
		// 저장 파일에는 override만 들어 있다.
		// 선언 정보는 Lua 파일에서 다시 읽어야 stale property를 정리할 수 있다.
		ScriptPropertyOverrides.clear();
		for (uint32 Index = 0; Index < OverrideCount; ++Index)
		{
			FString Name;
			FScriptPropertyValue Value;
			Ar << Name;
			Ar << Value;
			if (!Name.empty())
			{
				ScriptPropertyOverrides[Name] = Value;
			}
		}

		ScriptPath = FScriptPaths::NormalizeScriptPath(ScriptPath);
		ScriptInstance.SetScriptPath(ScriptPath);
		InvalidateScriptPropertyDescs();
	}
	else
	{
		// unordered_map 순서는 저장 파일마다 달라질 수 있지만, 각 항목이 name/value 쌍이라 로드 결과에는 영향이 없다.
		for (const auto& Pair : ScriptPropertyOverrides)
		{
			FString Name = Pair.first;
			FScriptPropertyValue Value = Pair.second;
			Ar << Name;
			Ar << Value;
		}
	}
}

void UScriptComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	Super::GetEditableProperties(OutProps);
	OutProps.push_back({ "ScriptPath", EPropertyType::String, &ScriptPath });

	if (!RefreshScriptPropertyDescs(false))
	{
		// property 스캔에 실패한 경우에는 오래된 UI 값을 보여주지 않는다.
		// 스크립트 로드 실패와 property 0개를 구분하기 위해 실패 상태는 내부에 남긴다.
		return;
	}

	for (const FScriptPropertyDesc& Desc : ScriptPropertyDescs)
	{
		// Details에 넘기는 값은 override 또는 default를 복사한 편집용 값이다.
		// 이 단계에서는 저장 상태를 바꾸지 않는다.
		auto ValueIt = EditableScriptPropertyValues.find(Desc.Name);
		if (ValueIt == EditableScriptPropertyValues.end())
		{
			continue;
		}

		FPropertyDescriptor Property;
		Property.Name = Desc.Name;
		Property.Type = ToEditablePropertyType(Desc.Type);
		Property.ValuePtr = GetEditableScriptPropertyValuePtr(ValueIt->second);
		Property.Speed = Desc.Type == EScriptPropertyType::Int ? 1.0f : 0.1f;
		if (Desc.bHasMin)
		{
			Property.Min = Desc.Min;
		}
		if (Desc.bHasMax)
		{
			Property.Max = Desc.Max;
		}
		OutProps.push_back(Property);
	}
}

void UScriptComponent::PostEditProperty(const char* PropertyName)
{
	Super::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "ScriptPath") == 0)
	{
		// 에디터에서 입력된 경로도 런타임과 같은 정규화 정책을 타게 만든다.
		SetScriptPath(ScriptPath);

		if (GetOwner() && GetOwner()->HasActorBegunPlay())
		{
			// 플레이 중 경로가 바뀌면 즉시 새 파일 기준으로 다시 로드한다.
			ReloadScript();
		}
	}
	else if (ApplyEditedScriptProperty(PropertyName))
	{
		// Details UI는 ValuePtr을 먼저 수정한 뒤 PostEditProperty를 호출한다.
		// 여기서만 override를 만들면 패널을 열어 보기만 해서는 저장 상태가 바뀌지 않는다.
		PruneScriptPropertyOverrides();
	}
}

void UScriptComponent::SetScriptPath(const FString& InPath)
{
	// 직렬화/에디터 입력/코드 호출 경로를 모두 같은 내부 표기로 맞춘다.
	ScriptPath = FScriptPaths::NormalizeScriptPath(InPath);
	ScriptInstance.SetScriptPath(ScriptPath);
	InvalidateScriptPropertyDescs();

	AActor* OwnerActor = GetOwner();
	if (OwnerActor && OwnerActor->HasActorBegunPlay())
	{
		// 플레이 도중 경로가 바뀌면 hot-reload 대상 매핑도 즉시 갱신되어야 한다.
		UpdateRuntimeRegistration();
	}
}

bool UScriptComponent::CreateScript()
{
	// 자동 생성 파일도 수동 입력 경로와 같은 정책을 타게 해서
	// 내부 저장 경로와 실제 파일 위치가 어긋나지 않게 유지한다.
	const std::filesystem::path ScriptsDir = std::filesystem::path(FPaths::ScriptsDir()).lexically_normal();
	std::error_code ErrorCode;
	std::filesystem::create_directories(ScriptsDir, ErrorCode);
	if (ErrorCode)
	{
		SetScriptError("CreateScript failed: could not create Scripts directory. " + ErrorCode.message());
		return false;
	}

	const std::filesystem::path TemplatePath = ScriptsDir / L"template.lua";
	if (!std::filesystem::exists(TemplatePath))
	{
		SetScriptError("CreateScript failed: Scripts/template.lua was not found.");
		return false;
	}

	const FString RelativePathString = ScriptPath.empty()
		? BuildDefaultScriptPath(GetOwner())
		: FScriptPaths::NormalizeScriptPath(ScriptPath);

	// 저장값은 상대 경로로 유지하고, 실제 복사 대상만 절대 경로로 바꿔 쓴다.
	const std::filesystem::path AbsoluteGeneratedPath = FScriptPaths::ResolveScriptPath(RelativePathString);

	ErrorCode.clear();

	std::filesystem::create_directories(AbsoluteGeneratedPath.parent_path(), ErrorCode);
	if (ErrorCode)
	{
		SetScriptError("CreateScript failed: could not create script directory. " + ErrorCode.message());
		return false;
	}

	if (std::filesystem::exists(AbsoluteGeneratedPath))
	{
		// 이미 파일이 있으면 덮어쓰지 않고 그 경로만 현재 component에 연결한다.
		UE_LOG("[ScriptComponent] CreateScript: existing script reused: %s", RelativePathString.c_str());
		SetScriptPath(RelativePathString);
		ClearScriptError();
		if (GetOwner() && GetOwner()->HasActorBegunPlay())
		{
			return ReloadScript();
		}
		return true;
	}

	ErrorCode.clear();
	const bool bCopied = std::filesystem::copy_file(
		TemplatePath,
		AbsoluteGeneratedPath,
		std::filesystem::copy_options::none,
		ErrorCode);

	if (!bCopied)
	{
		SetScriptError("CreateScript failed: " + ErrorCode.message());
		return false;
	}

	SetScriptPath(RelativePathString);
	ClearScriptError();

	if (GetOwner() && GetOwner()->HasActorBegunPlay())
	{
		return ReloadScript();
	}

	return true;
}

bool UScriptComponent::LoadScript()
{
	// 일반 로드는 항상 기존 Lua 상태를 비우고 처음부터 다시 시작한다.
	ScriptInstance.Shutdown();
	bLoaded = false;

	if (ScriptPath.empty())
	{
		SetScriptError("LoadScript failed: ScriptPath is empty.");
		return false;
	}

	InvalidateScriptPropertyDescs();
	RefreshScriptPropertyDescs();

	if (!ScriptInstance.Initialize(this))
	{
		SetScriptError(ScriptInstance.GetLastError());
		return false;
	}

	// ScriptInstance도 같은 canonical 경로를 갖고 있어야 이후 reload 비교가 단순해진다.
	ScriptInstance.SetScriptPath(ScriptPath);
	if (!ScriptInstance.LoadFromFile(ScriptPath))
	{
		SetScriptError(ScriptInstance.GetLastError());
		return false;
	}

	bLoaded = true;
	RefreshScriptErrorState();
	return true;
}

bool UScriptComponent::ReloadScript()
{
	// 수동 reload와 파일 변경 기반 hot-reload가 모두 이 경로를 사용한다.
	// 이미 owner가 연결된 경우에는 그 상태를 재사용하고, 아니면 다시 초기화한다.
	bLoaded = false;

	if (ScriptPath.empty())
	{
		SetScriptError("ReloadScript failed: ScriptPath is empty.");
		return false;
	}

	const std::filesystem::path AbsoluteScriptPath = FScriptPaths::ResolveScriptPath(ScriptPath);
	if (!std::filesystem::exists(AbsoluteScriptPath))
	{
		SetScriptError("ReloadScript failed: script file does not exist: " + FPaths::ToUtf8(AbsoluteScriptPath.generic_wstring()));
		return false;
	}

	InvalidateScriptPropertyDescs();
	RefreshScriptPropertyDescs();

	if (ScriptInstance.GetOwnerComponent() != this)
	{
		if (!ScriptInstance.Initialize(this))
		{
			SetScriptError(ScriptInstance.GetLastError());
			return false;
		}

		ScriptInstance.SetScriptPath(ScriptPath);
	}

	ScriptInstance.StopAllCoroutines();

	// 실제 environment 재구성은 ScriptInstance가 알고 있으므로
	// component는 reload 진입과 후처리만 담당한다.
	const bool bReloaded = ScriptInstance.Reload();
	bLoaded = bReloaded;
	if (!bReloaded)
	{
		if (ScriptInstance.HasError())
		{
			SetScriptError(ScriptInstance.GetLastError());
		}
		else
		{
			SetScriptError("ReloadScript failed: unknown Lua reload error.");
		}
		return false;
	}

	bool bBeginPlaySucceeded = true;
	if (GetOwner() && GetOwner()->HasActorBegunPlay())
	{
		// 현재 엔진 구조에서는 reload 후 BeginPlay를 다시 호출해
		// 스크립트가 자신의 초기 상태를 다시 세팅할 기회를 준다.
		bBeginPlaySucceeded = ScriptInstance.CallBeginPlay();
	}

	RefreshScriptErrorState();

	// BeginPlay가 실패한 경우 Load를 다시 수행한다
	if (!bBeginPlaySucceeded)
	{
		ScriptInstance.StopAllCoroutines();
		bLoaded = false;
	}
	return bReloaded && bBeginPlaySucceeded;
}

bool UScriptComponent::OpenScript()
{
	// 파일을 직접 편집할 수 있도록 절대 경로로 변환한 뒤 OS에 위임한다.
	if (ScriptPath.empty())
	{
		SetScriptError("OpenScript failed: ScriptPath is empty.");
		return false;
	}

	const std::filesystem::path AbsoluteScriptPath = FScriptPaths::ResolveScriptPath(ScriptPath);
	if (!std::filesystem::exists(AbsoluteScriptPath))
	{
		SetScriptError("OpenScript failed: script file does not exist: " + FPaths::ToUtf8(AbsoluteScriptPath.generic_wstring()));
		return false;
	}

	const std::wstring WidePath = AbsoluteScriptPath.wstring();

	const HINSTANCE Result = ShellExecuteW(
		nullptr,
		L"open",
		WidePath.c_str(),
		nullptr,
		nullptr,
		SW_SHOWNORMAL);

	if (reinterpret_cast<INT_PTR>(Result) <= 32)
	{
		SetScriptError("OpenScript failed: ShellExecuteW returned " + std::to_string(reinterpret_cast<INT_PTR>(Result)) + ".");
		return false;
	}

	ClearScriptError();
	return true;
}

void UScriptComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 로드되지 않은 스크립트는 Tick과 coroutine 갱신을 모두 건너뛴다.
	if (!bLoaded)
	{
		return;
	}

	// 일반 Tick 함수와 coroutine resume을 같은 프레임에서 순서대로 처리한다.
	// BeginPlay와 마찬가지로 호출을 실패한 경우 다시 load를 수행한다.
	if (!ScriptInstance.CallTick(DeltaTime))
	{
		RefreshScriptErrorState();

		ScriptInstance.StopAllCoroutines();
		bLoaded = false;

		return;
	}
	ScriptInstance.TickCoroutines(DeltaTime);
	RefreshScriptErrorState();
}

bool UScriptComponent::ResolveScriptPropertyValue(const FString& PropertyName, const FScriptPropertyValue& FallbackValue, bool bHasFallback, FScriptPropertyValue& OutValue)
{
	if (PropertyName.empty())
	{
		return false;
	}

	if (!RefreshScriptPropertyDescs())
	{
		// property 선언을 읽지 못해도 Lua 실행 자체는 fallback/default 인자로 계속 진행할 수 있게 둔다.
		// 단, 이미 저장된 override가 있으면 사용자 의도를 최대한 보존해 반환한다.
		const auto OverrideIt = ScriptPropertyOverrides.find(PropertyName);
		if (OverrideIt != ScriptPropertyOverrides.end())
		{
			OutValue = OverrideIt->second;
			return true;
		}

		if (bHasFallback)
		{
			OutValue = FallbackValue;
			return true;
		}

		return false;
	}

	const FScriptPropertyDesc* Desc = FindScriptPropertyDesc(PropertyName);
	const auto OverrideIt = ScriptPropertyOverrides.find(PropertyName);
	if (Desc)
	{
		// 우선순위: editor override -> 선언 default -> property 함수 fallback.
		if (OverrideIt != ScriptPropertyOverrides.end() && FScriptProperty::IsTypeCompatible(OverrideIt->second, Desc->Type))
		{
			OutValue = OverrideIt->second;
			ClampScriptPropertyValue(OutValue, *Desc);
			return true;
		}

		if (Desc->bHasDefault || !bHasFallback)
		{
			OutValue = Desc->DefaultValue;
			return true;
		}

		OutValue = CoerceScriptPropertyValue(FallbackValue, Desc->Type, Desc->DefaultValue);
		ClampScriptPropertyValue(OutValue, *Desc);
		return true;
	}

	if (OverrideIt != ScriptPropertyOverrides.end())
	{
		OutValue = OverrideIt->second;
		return true;
	}

	if (bHasFallback)
	{
		OutValue = FallbackValue;
		return true;
	}

	return false;
}

bool UScriptComponent::RefreshScriptProperties()
{
	InvalidateScriptPropertyDescs();
	return RefreshScriptPropertyDescs();
}

void UScriptComponent::InvalidateScriptPropertyDescs()
{
	// ScriptPath 변경, load/reload 진입 시 호출된다.
	// 다음 Details 표시나 property() 호출 때 최신 Lua 선언을 다시 읽게 만든다.
	ScriptPropertyDescs.clear();
	EditableScriptPropertyValues.clear();
	LastScriptPropertyError.clear();
	bScriptPropertyDescsLoaded = false;
	bScriptPropertyDescsValid = false;
}

bool UScriptComponent::RefreshScriptPropertyDescs(bool bLogFailure)
{
	if (bScriptPropertyDescsLoaded)
	{
		// 같은 캐시 주기에서는 Lua 파일을 반복 실행하지 않는다.
		// Details 패널은 매 프레임 그려질 수 있으므로 이 가드가 필요하다.
		return bScriptPropertyDescsValid;
	}

	bScriptPropertyDescsLoaded = true;
	bScriptPropertyDescsValid = false;
	ScriptPropertyDescs.clear();
	EditableScriptPropertyValues.clear();
	LastScriptPropertyError.clear();

	if (ScriptPath.empty())
	{
		bScriptPropertyDescsValid = true;
		PruneScriptPropertyOverrides();
		return true;
	}

	FString LoadError;
	TArray<FScriptPropertyDesc> LoadedDescs;
	if (!FScriptProperty::LoadDescs(ScriptPath, LoadedDescs, LoadError))
	{
		// 스크립트 실행 실패와 "정상적으로 property 0개"를 구분하기 위해 실패 상태를 별도 보관한다.
		LastScriptPropertyError = LoadError;
		if (bLogFailure)
		{
			UE_LOG("[ScriptComponent] Script property scan failed: %s", LoadError.c_str());
		}
		return false;
	}

	ScriptPropertyDescs = std::move(LoadedDescs);
	bScriptPropertyDescsValid = true;
	// 선언이 바뀐 뒤 남은 override를 먼저 정리하고, 그 결과를 UI 편집값에 반영한다.
	PruneScriptPropertyOverrides();
	RebuildEditableScriptPropertyValues();
	return true;
}

void UScriptComponent::RebuildEditableScriptPropertyValues()
{
	EditableScriptPropertyValues.clear();
	for (const FScriptPropertyDesc& Desc : ScriptPropertyDescs)
	{
		// UI에는 "현재 적용될 값"을 보여준다. override가 없으면 선언 default가 그대로 보인다.
		FScriptPropertyValue Value = Desc.DefaultValue;
		const auto OverrideIt = ScriptPropertyOverrides.find(Desc.Name);
		if (OverrideIt != ScriptPropertyOverrides.end() && FScriptProperty::IsTypeCompatible(OverrideIt->second, Desc.Type))
		{
			Value = OverrideIt->second;
		}

		ClampScriptPropertyValue(Value, Desc);
		EditableScriptPropertyValues[Desc.Name] = Value;
	}
}

void UScriptComponent::PruneScriptPropertyOverrides()
{
	for (auto It = ScriptPropertyOverrides.begin(); It != ScriptPropertyOverrides.end();)
	{
		// 삭제된 선언, 타입이 바뀐 선언, default와 같은 값은 저장할 필요가 없다.
		const FScriptPropertyDesc* Desc = FindScriptPropertyDesc(It->first);
		if (!Desc || !FScriptProperty::IsTypeCompatible(It->second, Desc->Type))
		{
			It = ScriptPropertyOverrides.erase(It);
			continue;
		}

		ClampScriptPropertyValue(It->second, *Desc);
		if (FScriptProperty::AreValuesEqual(It->second, Desc->DefaultValue))
		{
			It = ScriptPropertyOverrides.erase(It);
			continue;
		}

		++It;
	}
}

bool UScriptComponent::ApplyEditedScriptProperty(const FString& PropertyName)
{
	auto EditedValueIt = EditableScriptPropertyValues.find(PropertyName);
	if (EditedValueIt == EditableScriptPropertyValues.end())
	{
		return false;
	}

	const FScriptPropertyDesc* Desc = FindScriptPropertyDesc(PropertyName);
	if (!Desc)
	{
		ScriptPropertyOverrides.erase(PropertyName);
		EditableScriptPropertyValues.erase(EditedValueIt);
		return true;
	}

	FScriptPropertyValue EditedValue = EditedValueIt->second;
	EditedValue.Type = Desc->Type;
	ClampScriptPropertyValue(EditedValue, *Desc);
	EditedValueIt->second = EditedValue;

	// 사용자가 default와 같은 값으로 되돌리면 override를 제거한다.
	// 이 규칙 덕분에 Details를 열고 닫기만 해도 저장 데이터가 늘어나는 일이 없다.
	if (FScriptProperty::AreValuesEqual(EditedValue, Desc->DefaultValue))
	{
		ScriptPropertyOverrides.erase(PropertyName);
	}
	else
	{
		ScriptPropertyOverrides[PropertyName] = EditedValue;
	}

	return true;
}

const FScriptPropertyDesc* UScriptComponent::FindScriptPropertyDesc(const FString& PropertyName) const
{
	const auto It = std::find_if(
		ScriptPropertyDescs.begin(),
		ScriptPropertyDescs.end(),
		[&PropertyName](const FScriptPropertyDesc& Desc)
		{
			return Desc.Name == PropertyName;
		});
	return It != ScriptPropertyDescs.end() ? &(*It) : nullptr;
}

void UScriptComponent::ClearScriptError()
{
	bHasScriptError = false;
	LastScriptError.clear();
}

void UScriptComponent::SetScriptError(const FString& ErrorMessage)
{
	bHasScriptError = true;
	LastScriptError = ErrorMessage;
	UE_LOG("[ScriptComponent] %s", ErrorMessage.c_str());
}

void UScriptComponent::RefreshScriptErrorState()
{
	// 실제 Lua 실행 에러는 ScriptInstance가 쥐고 있으므로
	// component는 그 상태를 UI/로그용으로 반영만 한다.
	if (ScriptInstance.HasError())
	{
		SetScriptError(ScriptInstance.GetLastError());
		return;
	}

	ClearScriptError();
}

void UScriptComponent::UpdateRuntimeRegistration()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasActorBegunPlay() || ScriptPath.empty())
	{
		// 아직 플레이 전이거나 경로가 없으면 watcher 대상에서 뺀다.
		FLuaScriptRuntime::Get().UnregisterScriptComponent(this);
		return;
	}

	// 등록은 로딩 성공 여부와 별개로 유지한다.
	// 그래야 파일이 나중에 생성되거나 수정됐을 때도 같은 경로 기준으로 reload 시도 가능하다.
	FLuaScriptRuntime::Get().RegisterScriptComponent(this);
}
