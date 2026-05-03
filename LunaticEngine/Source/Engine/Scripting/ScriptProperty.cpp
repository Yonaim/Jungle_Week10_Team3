#include "Scripting/ScriptProperty.h"

#include "Core/Log.h"
#include "Platform/Paths.h"
#include "Platform/ScriptPaths.h"
#include "Serialization/Archive.h"

#pragma region SolInclude
#ifdef check
#pragma push_macro("check")
#undef check
#define SCRIPT_PROPERTY_RESTORE_CHECK_MACRO
#endif

#ifdef checkf
#pragma push_macro("checkf")
#undef checkf
#define SCRIPT_PROPERTY_RESTORE_CHECKF_MACRO
#endif

#include <sol/sol.hpp>

#ifdef SCRIPT_PROPERTY_RESTORE_CHECKF_MACRO
#pragma pop_macro("checkf")
#undef SCRIPT_PROPERTY_RESTORE_CHECKF_MACRO
#endif

#ifdef SCRIPT_PROPERTY_RESTORE_CHECK_MACRO
#pragma pop_macro("check")
#undef SCRIPT_PROPERTY_RESTORE_CHECK_MACRO
#endif
#pragma endregion

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace
{
	constexpr float PropertyFloatTolerance = 0.0001f;

	FScriptPropertyValue ReadDefaultValue(sol::object DefaultObject, EScriptPropertyType Type)
	{
		// default가 없거나 타입이 맞지 않으면 타입별 0값을 사용한다.
		// 선언 파싱 단계에서 스크립트를 실패시키지 않고, 잘못된 항목만 안전한 기본값으로 둔다.
		FScriptPropertyValue Value = FScriptProperty::MakeDefaultValue(Type);
		if (!DefaultObject.valid() || DefaultObject == sol::lua_nil)
		{
			return Value;
		}

		switch (Type)
		{
		case EScriptPropertyType::Bool:
			if (DefaultObject.get_type() == sol::type::boolean)
			{
				Value.BoolValue = DefaultObject.as<bool>();
			}
			break;
		case EScriptPropertyType::Int:
			if (DefaultObject.get_type() == sol::type::number)
			{
				Value.IntValue = DefaultObject.as<int32>();
			}
			break;
		case EScriptPropertyType::Float:
			if (DefaultObject.get_type() == sol::type::number)
			{
				Value.FloatValue = DefaultObject.as<float>();
			}
			break;
		case EScriptPropertyType::String:
			if (DefaultObject.get_type() == sol::type::string)
			{
				Value.StringValue = DefaultObject.as<FString>();
			}
			break;
		case EScriptPropertyType::Vector:
			if (DefaultObject.get_type() == sol::type::table)
			{
				// 실제 vec3 userdata가 없는 스캔 환경에서도 {x,y,z}, {X,Y,Z}, 배열형 테이블을 모두 허용한다.
				sol::table Table = DefaultObject.as<sol::table>();
				Value.VectorValue.X = Table["x"].get_or(Table["X"].get_or(Table[1].get_or(0.0f)));
				Value.VectorValue.Y = Table["y"].get_or(Table["Y"].get_or(Table[2].get_or(0.0f)));
				Value.VectorValue.Z = Table["z"].get_or(Table["Z"].get_or(Table[3].get_or(0.0f)));
			}
			break;
		default:
			break;
		}

		return Value;
	}

	FString ToLower(FString Value)
	{
		std::transform(
			Value.begin(),
			Value.end(),
			Value.begin(),
			[](unsigned char Character)
			{
				return static_cast<char>(std::tolower(Character));
			});
		return Value;
	}

	FString MakeLuaPath(const std::filesystem::path& Path)
	{
		return FPaths::ToUtf8(Path.generic_wstring());
	}

	void AddScriptPackagePaths(sol::state& Lua)
	{
		// property 선언 파일이 require를 쓰는 경우를 위해 런타임과 같은 Scripts 검색 경로만 사용한다.
		// 단, 실제 게임 바인딩은 넣지 않고 스캔에 필요한 최소 함수만 별도로 stub 처리한다.
		sol::table Package = Lua["package"];
		if (!Package.valid())
		{
			return;
		}

		const std::filesystem::path ScriptsDir(FPaths::ScriptsDir());
		FString NewPath = MakeLuaPath(ScriptsDir / L"?.lua");
		NewPath += ";";
		NewPath += MakeLuaPath(ScriptsDir / L"?" / L"init.lua");
		Package["path"] = NewPath;
		Package["cpath"] = "";

		// require로 불러온 모듈은 Lua state 안에서 캐시됩니다.
		// 여러 스크립트가 같은 모듈 테이블을 공유하므로 Config는 상수처럼 사용하는 것이 안전합니다.
	}

	bool InstallPropertyScanStubs(sol::state& Lua, sol::environment& Env, FString& OutError)
	{
		// property 선언만 읽는 전용 실행에서는 실제 Actor/입력/코루틴이 없다.
		// top-level 보조 함수 호출이 descriptor 파싱을 막지 않도록 최소 stub만 심는다.
		static constexpr const char* StubSource = R"(
			local __dummy
			__dummy = setmetatable({}, {
			    __index = function()
			        return function()
			            return __dummy
			        end
			    end,
			    __newindex = function() end
			})
			
			obj = __dummy
			
			function vec3(x, y, z)
			    x = x or 0
			    y = y or 0
			    z = z or 0
			    return { x = x, y = y, z = z, X = x, Y = y, Z = z, [1] = x, [2] = y, [3] = z }
			end
			
			Vector = { new = vec3 }
			
			function log(...) end
			function warn(...) end
			function error_log(...) end
			function print(...) end
			function property(_, defaultValue) return defaultValue end
			function time() return 0 end
			function delta_time() return 0 end
			function key() return false end
			function key_down() return false end
			function key_up() return false end
			function start_coroutine() return false end
			function wait() return nil end
			function wait_frames() return nil end
			)";

		sol::protected_function_result StubResult = Lua.safe_script(
			StubSource,
			Env,
			sol::script_pass_on_error,
			"ScriptPropertyStubs",
			sol::load_mode::text);

		if (!StubResult.valid())
		{
			const sol::optional<sol::error> MaybeError = StubResult.get<sol::optional<sol::error>>();
			OutError = MaybeError ? MaybeError->what() : FString("Unknown Lua property stub error.");
			return false;
		}

		return true;
	}
}

bool FScriptProperty::LoadDescs(const FString& ScriptPath, TArray<FScriptPropertyDesc>& OutDescs, FString& OutError)
{
	OutDescs.clear();
	OutError.clear();

	const FString NormalizedPath = FScriptPaths::NormalizeScriptPath(ScriptPath);
	if (NormalizedPath.empty())
	{
		OutError = "Script path is empty.";
		return false;
	}

	FString ScriptSource;
	FString FileError;
	if (!FScriptPaths::ReadScriptFile(NormalizedPath, ScriptSource, FileError))
	{
		OutError = FileError;
		return false;
	}

	sol::state Lua;
	Lua.open_libraries(
		sol::lib::base,
		sol::lib::math,
		sol::lib::string,
		sol::lib::table,
		sol::lib::package);
	AddScriptPackagePaths(Lua);

	sol::environment Env(Lua, sol::create, Lua.globals());
	if (!InstallPropertyScanStubs(Lua, Env, OutError))
	{
		return false;
	}

	Env.set_function("DeclareProperties", [&OutDescs](sol::table PropertyTable)
	{
		// 한 파일 안에서 DeclareProperties를 여러 번 호출하면 마지막 선언을 기준으로 본다.
		// 템플릿/실험 중 중복 선언이 있어도 UI에는 한 벌만 노출되게 하기 위함이다.
		OutDescs.clear();

		for (const auto& Pair : PropertyTable)
		{
			const sol::object KeyObject = Pair.first;
			const sol::object DescObject = Pair.second;
			if (KeyObject.get_type() != sol::type::string || DescObject.get_type() != sol::type::table)
			{
				continue;
			}

			const FString Name = KeyObject.as<FString>();
			sol::table DescTable = DescObject.as<sol::table>();

			EScriptPropertyType Type = EScriptPropertyType::Float;
			const FString TypeName = DescTable["type"].get_or(FString("float"));
			if (!FScriptProperty::TryParseType(TypeName, Type))
			{
				UE_LOG("[ScriptProperty] Unknown property type '%s' for '%s'.", TypeName.c_str(), Name.c_str());
				continue;
			}

			FScriptPropertyDesc Desc;
			Desc.Name = Name;
			Desc.Type = Type;
			const sol::object DefaultObject = DescTable["default"];
			Desc.bHasDefault = DefaultObject.valid() && DefaultObject != sol::lua_nil;
			Desc.DefaultValue = ReadDefaultValue(DefaultObject, Type);

			if (DescTable["min"].valid() && DescTable["min"].get_type() == sol::type::number)
			{
				Desc.bHasMin = true;
				Desc.Min = DescTable["min"].get_or(0.0f);
			}

			if (DescTable["max"].valid() && DescTable["max"].get_type() == sol::type::number)
			{
				Desc.bHasMax = true;
				Desc.Max = DescTable["max"].get_or(0.0f);
			}

			OutDescs.push_back(Desc);
		}
	});

	const std::filesystem::path ResolvedPath = FScriptPaths::ResolveScriptPath(NormalizedPath);
	const FString ChunkName = FPaths::ToUtf8(ResolvedPath.generic_wstring());
	sol::protected_function_result Result = Lua.safe_script(
		ScriptSource,
		Env,
		sol::script_pass_on_error,
		ChunkName,
		sol::load_mode::text);

	if (!Result.valid())
	{
		const sol::optional<sol::error> MaybeError = Result.get<sol::optional<sol::error>>();
		OutError = MaybeError ? MaybeError->what() : FString("Unknown Lua property error.");
		UE_LOG("[ScriptProperty] %s", OutError.c_str());
		return false;
	}

	std::sort(
		OutDescs.begin(),
		OutDescs.end(),
		[](const FScriptPropertyDesc& Left, const FScriptPropertyDesc& Right)
		{
			return Left.Name < Right.Name;
		});
	// Lua table 순회 순서는 안정적이지 않으므로 이름순으로 정렬해 Details UI 표시 순서를 고정한다.
	return true;
}

bool FScriptProperty::TryParseType(const FString& TypeName, EScriptPropertyType& OutType)
{
	const FString LowerName = ToLower(TypeName);
	if (LowerName == "bool")
	{
		OutType = EScriptPropertyType::Bool;
		return true;
	}
	if (LowerName == "int")
	{
		OutType = EScriptPropertyType::Int;
		return true;
	}
	if (LowerName == "float")
	{
		OutType = EScriptPropertyType::Float;
		return true;
	}
	if (LowerName == "string")
	{
		OutType = EScriptPropertyType::String;
		return true;
	}
	if (LowerName == "vector" || LowerName == "vec3")
	{
		OutType = EScriptPropertyType::Vector;
		return true;
	}

	return false;
}

FScriptPropertyValue FScriptProperty::MakeDefaultValue(EScriptPropertyType Type)
{
	FScriptPropertyValue Value;
	Value.Type = Type;
	return Value;
}

bool FScriptProperty::IsTypeCompatible(const FScriptPropertyValue& Value, EScriptPropertyType Type)
{
	return Value.Type == Type;
}

bool FScriptProperty::AreValuesEqual(const FScriptPropertyValue& Left, const FScriptPropertyValue& Right)
{
	// Details에서 float/vector를 드래그하면 아주 작은 오차가 생길 수 있다.
	// default와 같은 override를 지울 때 이 오차 때문에 불필요한 저장값이 남지 않도록 허용 오차를 둔다.
	if (Left.Type != Right.Type)
	{
		return false;
	}

	switch (Left.Type)
	{
	case EScriptPropertyType::Bool:
		return Left.BoolValue == Right.BoolValue;
	case EScriptPropertyType::Int:
		return Left.IntValue == Right.IntValue;
	case EScriptPropertyType::Float:
		return std::fabs(Left.FloatValue - Right.FloatValue) <= PropertyFloatTolerance;
	case EScriptPropertyType::String:
		return Left.StringValue == Right.StringValue;
	case EScriptPropertyType::Vector:
		return std::fabs(Left.VectorValue.X - Right.VectorValue.X) <= PropertyFloatTolerance
			&& std::fabs(Left.VectorValue.Y - Right.VectorValue.Y) <= PropertyFloatTolerance
			&& std::fabs(Left.VectorValue.Z - Right.VectorValue.Z) <= PropertyFloatTolerance;
	default:
		return false;
	}
}

FArchive& operator<<(FArchive& Ar, FScriptPropertyValue& Value)
{
	Ar << Value.Type;
	Ar << Value.BoolValue;
	Ar << Value.IntValue;
	Ar << Value.FloatValue;
	Ar << Value.StringValue;
	Ar << Value.VectorValue;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FScriptPropertyDesc& Desc)
{
	Ar << Desc.Name;
	Ar << Desc.Type;
	Ar << Desc.DefaultValue;
	Ar << Desc.bHasDefault;
	Ar << Desc.bHasMin;
	Ar << Desc.bHasMax;
	Ar << Desc.Min;
	Ar << Desc.Max;
	return Ar;
}
