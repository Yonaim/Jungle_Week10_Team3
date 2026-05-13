#include "PCH/LunaticPCH.h"
#include "Engine/Asset/AssetFileSerializer.h"

#include "Engine/Asset/AssetData.h"
#include "Core/Log.h"
#include "Materials/Material.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/StaticMesh.h"
#include "Texture/Texture2D.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"
#include "Object/ObjectFactory.h"

#include <chrono>

namespace
{
    uint32 GCurrentAssetSerializationVersion = FAssetFileSerializer::AssetVersion;

    void WriteError(FString* OutError, const FString& Message)
    {
        if (OutError)
        {
            *OutError = Message;
        }
    }

    std::filesystem::path CanonicalizeAssetLayoutPath(std::filesystem::path Path)
    {
        FString Generic = FPaths::ToUtf8(Path.generic_wstring());

        auto ReplacePrefix = [&Generic](const FString& OldPrefix, const FString& NewPrefix)
        {
            if (Generic.rfind(OldPrefix, 0) == 0)
            {
                Generic = NewPrefix + Generic.substr(OldPrefix.size());
                return;
            }

            const FString Segment = "/" + OldPrefix;
            const size_t SegmentPos = Generic.find(Segment);
            if (SegmentPos != FString::npos)
            {
                Generic.replace(SegmentPos + 1, OldPrefix.size(), NewPrefix);
            }
        };

        ReplacePrefix("Asset/Engine/Content", "Asset/Content");
        ReplacePrefix("Asset/Game/Content", "Asset/Content");
        ReplacePrefix("Asset/Engine/Source", "Asset/Source");
        ReplacePrefix("Asset/Game/Source", "Asset/Source");

        return std::filesystem::path(FPaths::ToWide(Generic)).lexically_normal();
    }

    std::filesystem::path ResolveAssetFilePathForIO(const std::filesystem::path& FilePath)
    {
        std::filesystem::path ResolvedPath = CanonicalizeAssetLayoutPath(FilePath.lexically_normal());
        if (!ResolvedPath.is_absolute())
        {
            ResolvedPath = (std::filesystem::path(FPaths::RootDir()) / ResolvedPath).lexically_normal();
        }
        return ResolvedPath;
    }

    FString ToAssetPathString(const std::filesystem::path& FilePath)
    {
        return FPaths::ToUtf8(FilePath.lexically_normal().generic_wstring());
    }

    FString ToProjectRelativeAssetPathString(const std::filesystem::path& FilePath)
    {
        const std::filesystem::path NormalizedPath = FilePath.lexically_normal();
        const std::filesystem::path ProjectRoot = std::filesystem::path(FPaths::RootDir()).lexically_normal();
        const std::filesystem::path RelativePath = NormalizedPath.lexically_relative(ProjectRoot);
        if (!RelativePath.empty() && RelativePath.native().find(L"..") != 0)
        {
            return FPaths::ToUtf8(RelativePath.generic_wstring());
        }
        return ToAssetPathString(NormalizedPath);
    }

    FString MakeAssetGuidString()
    {
        static uint64 Counter = 1;
        const uint64 TimeSeed = static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        return std::to_string(TimeSeed) + "-" + std::to_string(Counter++);
    }

    FString MakeAssetName(const std::filesystem::path& FilePath, UObject* Object)
    {
        if (Object && !Object->GetFName().ToString().empty())
        {
            return Object->GetFName().ToString();
        }
        return FPaths::ToUtf8(FilePath.stem().wstring());
    }

    void SerializeHeader(FArchive& Ar, FAssetFileHeader& Header)
    {
        Ar << Header.Magic;
        Ar << Header.FileVersion;
        Ar << Header.ClassId;
        Ar << Header.ClassName;
        Ar << Header.AssetName;
        Ar << Header.AssetGuid;
        Ar << Header.DependencyCount;
        Ar << Header.BodyOffset;
    }
} // namespace

namespace FAssetFileSerializer
{
    EAssetClassId GetAssetClassIdFromClassName(const FString& ClassName)
    {
        if (ClassName == "UStaticMesh") return EAssetClassId::StaticMesh;
        if (ClassName == "USkeletalMesh") return EAssetClassId::SkeletalMesh;
        if (ClassName == "UMaterial") return EAssetClassId::Material;
        if (ClassName == "UTexture2D") return EAssetClassId::Texture;
        if (ClassName == "UCameraModifierStackAssetData") return EAssetClassId::CameraModifierStack;
        if (ClassName == "USkeletonPoseAsset") return EAssetClassId::PoseAsset;
        return EAssetClassId::Unknown;
    }

    FString GetClassNameFromAssetClassId(EAssetClassId ClassId)
    {
        switch (ClassId)
        {
        case EAssetClassId::StaticMesh: return "UStaticMesh";
        case EAssetClassId::SkeletalMesh: return "USkeletalMesh";
        case EAssetClassId::Material: return "UMaterial";
        case EAssetClassId::Texture: return "UTexture2D";
        case EAssetClassId::CameraModifierStack: return "UCameraModifierStackAssetData";
        case EAssetClassId::PoseAsset: return "USkeletonPoseAsset";
        default: return "";
        }
    }

    bool ReadAssetHeader(const std::filesystem::path& FilePath, FAssetFileHeader& OutHeader, FString* OutError)
    {
        const std::filesystem::path ResolvedPath = ResolveAssetFilePathForIO(FilePath);
        FWindowsBinReader Reader(ToAssetPathString(ResolvedPath));
        if (!Reader.IsValid())
        {
            WriteError(OutError, "ReadAssetHeader failed: Could not open file.");
            return false;
        }

        Reader << OutHeader.Magic;
        Reader << OutHeader.FileVersion;

        if (OutHeader.Magic != AssetMagic)
        {
            WriteError(OutError, "ReadAssetHeader failed: Unknown .uasset binary format.");
            return false;
        }
        if (OutHeader.FileVersion == 0 || OutHeader.FileVersion > AssetVersion)
        {
            WriteError(OutError, "ReadAssetHeader failed: Unsupported .uasset version.");
            return false;
        }

        if (OutHeader.FileVersion < 3)
        {
            Reader << OutHeader.ClassName;
            OutHeader.ClassId = GetAssetClassIdFromClassName(OutHeader.ClassName);
            OutHeader.AssetName = FPaths::ToUtf8(ResolvedPath.stem().wstring());
            OutHeader.AssetGuid.clear();
            OutHeader.DependencyCount = 0;
            OutHeader.BodyOffset = 0;
            return true;
        }

        Reader << OutHeader.ClassId;
        Reader << OutHeader.ClassName;
        Reader << OutHeader.AssetName;
        Reader << OutHeader.AssetGuid;
        Reader << OutHeader.DependencyCount;
        Reader << OutHeader.BodyOffset;
        return true;
    }

    bool SaveObjectToAssetFile(const std::filesystem::path& FilePath, UObject* RootObject, FString* OutError)
    {
        const std::filesystem::path ResolvedPath = ResolveAssetFilePathForIO(FilePath);
        const FString AssetPath = ToAssetPathString(ResolvedPath);
        const FString DisplayAssetPath = ToProjectRelativeAssetPathString(ResolvedPath);
        if (!RootObject)
        {
            WriteError(OutError, "Save failed: RootObject is null.");
            UE_LOG_CATEGORY(AssetFileSerializer, Error, "[AssetSave] Failed: path=%s error=RootObject is null", DisplayAssetPath.c_str());
            return false;
        }

        UE_LOG_CATEGORY(AssetFileSerializer, Info, "[AssetSave] Begin: path=%s class=%s", DisplayAssetPath.c_str(), RootObject->GetClass()->GetName());
        if (ResolvedPath.has_parent_path())
        {
            std::filesystem::create_directories(ResolvedPath.parent_path());
        }
        FWindowsBinWriter Writer(AssetPath);
        if (!Writer.IsValid())
        {
            WriteError(OutError, "Save failed: Could not open file for writing.");
            UE_LOG_CATEGORY(AssetFileSerializer, Error, "[AssetSave] Failed: path=%s error=Could not open file for writing", DisplayAssetPath.c_str());
            return false;
        }

        FAssetFileHeader Header;
        Header.Magic = AssetMagic;
        Header.FileVersion = AssetVersion;
        Header.ClassName = RootObject->GetClass()->GetName();
        Header.ClassId = GetAssetClassIdFromClassName(Header.ClassName);
        Header.AssetName = MakeAssetName(ResolvedPath, RootObject);
        Header.AssetGuid = MakeAssetGuidString();
        Header.DependencyCount = 0;
        Header.BodyOffset = 0; // 현재 Archive는 seek/tell이 없으므로 header 직후 body 규약으로 사용한다.

        SerializeHeader(Writer, Header);
        GCurrentAssetSerializationVersion = Header.FileVersion;
        RootObject->Serialize(Writer);
        GCurrentAssetSerializationVersion = AssetVersion;
        UE_LOG_CATEGORY(AssetFileSerializer, Info, "[AssetSave] Complete: path=%s class=%s", DisplayAssetPath.c_str(), Header.ClassName.c_str());
        return true;
    }

    UObject* LoadObjectFromAssetFile(const std::filesystem::path& FilePath, FString* OutError)
    {
        const std::filesystem::path ResolvedPath = ResolveAssetFilePathForIO(FilePath);
        const FString AssetPath = ToAssetPathString(ResolvedPath);
        const FString DisplayAssetPath = ToProjectRelativeAssetPathString(ResolvedPath);
        UE_LOG_CATEGORY(AssetFileSerializer, Info, "[AssetLoad] Begin: path=%s", DisplayAssetPath.c_str());
        FWindowsBinReader Reader(AssetPath);
        if (!Reader.IsValid())
        {
            WriteError(OutError, "Load failed: Could not open file.");
            UE_LOG_CATEGORY(AssetFileSerializer, Error, "[AssetLoad] Failed: path=%s error=Could not open file", DisplayAssetPath.c_str());
            return nullptr;
        }

        FAssetFileHeader Header;
        Reader << Header.Magic;
        Reader << Header.FileVersion;

        if (Header.Magic != AssetMagic)
        {
            WriteError(OutError, "Load failed: Unknown .uasset binary format.");
            UE_LOG_CATEGORY(AssetFileSerializer, Error, "[AssetLoad] Failed: path=%s error=Unknown .uasset binary format", DisplayAssetPath.c_str());
            return nullptr;
        }
        if (Header.FileVersion == 0 || Header.FileVersion > AssetVersion)
        {
            WriteError(OutError, "Load failed: Unsupported .uasset version.");
            UE_LOG_CATEGORY(AssetFileSerializer, Error, "[AssetLoad] Failed: path=%s error=Unsupported .uasset version=%u", DisplayAssetPath.c_str(), Header.FileVersion);
            return nullptr;
        }

        if (Header.FileVersion < 3)
        {
            Reader << Header.ClassName;
            Header.ClassId = GetAssetClassIdFromClassName(Header.ClassName);
            Header.AssetName = FPaths::ToUtf8(ResolvedPath.stem().wstring());
        }
        else
        {
            Reader << Header.ClassId;
            Reader << Header.ClassName;
            Reader << Header.AssetName;
            Reader << Header.AssetGuid;
            Reader << Header.DependencyCount;
            Reader << Header.BodyOffset;
        }

        FString ClassName = Header.ClassName.empty() ? GetClassNameFromAssetClassId(Header.ClassId) : Header.ClassName;
        UObject* Object = FObjectFactory::Get().Create(ClassName);
        if (!Object)
        {
            WriteError(OutError, "Load failed: Unknown asset class: " + ClassName);
            UE_LOG_CATEGORY(AssetFileSerializer, Error, "[AssetLoad] Failed: path=%s error=Unknown asset class=%s", DisplayAssetPath.c_str(), ClassName.c_str());
            return nullptr;
        }

        if (!Header.AssetName.empty())
        {
            Object->SetFName(FName(Header.AssetName));
        }

        GCurrentAssetSerializationVersion = Header.FileVersion;
        Object->Serialize(Reader);
        GCurrentAssetSerializationVersion = AssetVersion;
        UE_LOG_CATEGORY(AssetFileSerializer, Info, "[AssetLoad] Complete: path=%s class=%s name=%s", DisplayAssetPath.c_str(), ClassName.c_str(), Header.AssetName.c_str());
        return Object;
    }

    bool SaveAssetToFile(const std::filesystem::path& FilePath, UAssetData* Asset, FString* OutError)
    {
        return SaveObjectToAssetFile(FilePath, Asset, OutError);
    }

    UAssetData* LoadAssetFromFile(const std::filesystem::path& FilePath, FString* OutError)
    {
        UObject* Object = LoadObjectFromAssetFile(FilePath, OutError);
        UAssetData* Asset = Cast<UAssetData>(Object);
        if (!Asset && Object)
        {
            UObjectManager::Get().DestroyObject(Object);
            WriteError(OutError, "Load failed: File root object is not UAssetData.");
        }
        return Asset;
    }

    uint32 GetCurrentAssetSerializationVersion()
    {
        return GCurrentAssetSerializationVersion;
    }
} // namespace FAssetFileSerializer
