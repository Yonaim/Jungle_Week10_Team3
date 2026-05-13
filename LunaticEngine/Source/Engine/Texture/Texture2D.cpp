#include "PCH/LunaticPCH.h"
#include "Texture/Texture2D.h"
#include "Object/ObjectFactory.h"
#include "Core/AsciiUtils.h"
#include "Core/Log.h"
#include "Core/Notification.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Platform/DirectoryWatcher.h"
#include "Platform/Paths.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <cwctype>
#include <d3d11.h>
#include <filesystem>
#include <wincodec.h>

IMPLEMENT_CLASS(UTexture2D, UObject)

std::map<FString, UTexture2D*> UTexture2D::TextureCache;
TArray<FTextureAssetListItem> UTexture2D::AvailableTextureFiles;
bool UTexture2D::bTextureAssetListDirty = true;
TSet<FString> UTexture2D::PendingTextureRefreshPaths;
FWatchID UTexture2D::TextureAssetWatchID = 0;
FSubscriptionID UTexture2D::TextureAssetWatchSub = 0;
bool UTexture2D::bTextureAssetWatcherInitialized = false;

namespace
{
	void NotifyTextureImport(const FString& Message, ENotificationType Type = ENotificationType::Info, float Duration = 3.0f)
	{
		FNotificationManager::Get().AddNotification(Message, Type, Duration);
	}

	FString GetFileNameUtf8(const FString& Path)
	{
		return FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(Path)).filename().wstring());
	}

	FString NormalizeTexturePath(const FString& FilePath)
	{
		return FPaths::NormalizePath(FilePath);
	}

	std::wstring ResolveTexturePathOnDisk(const FString& FilePath)
	{
		return FPaths::ResolvePathToDisk(FilePath);
	}


	std::filesystem::path FindTextureSourceOnDisk(const FString& SourcePath)
	{
		std::vector<std::filesystem::path> Candidates;
		auto AddCandidate = [&Candidates](const std::filesystem::path& Candidate)
		{
			if (Candidate.empty())
			{
				return;
			}
			const std::filesystem::path Normalized = Candidate.lexically_normal();
			if (std::find(Candidates.begin(), Candidates.end(), Normalized) == Candidates.end())
			{
				Candidates.push_back(Normalized);
			}
		};

		const std::filesystem::path ProjectRoot(FPaths::RootDir());
		const std::filesystem::path Original(FPaths::ToWide(SourcePath));
		const std::filesystem::path Resolved(FPaths::ResolvePathToDisk(SourcePath));

		AddCandidate(Resolved);
		AddCandidate(Original.is_absolute() ? Original : ProjectRoot / Original);

		// FBX exporters sometimes write paths like
		// ../../../../Github/.../LunaticEngine/Asset/Content/Model/Tex.png.
		// If that relative path is resolved from the executable/root directory it points to
		// the wrong place.  Once an Asset/... segment is present, remap that segment to the
		// current project root and try it before giving up.
		std::wstring SlashPath = FPaths::ToWide(SourcePath);
		std::replace(SlashPath.begin(), SlashPath.end(), L'\\', L'/');
		auto AddFromProjectSegment = [&](const std::wstring& Marker)
		{
			const size_t Pos = SlashPath.find(Marker);
			if (Pos != std::wstring::npos)
			{
				AddCandidate(ProjectRoot / std::filesystem::path(SlashPath.substr(Pos)));
			}
		};
		AddFromProjectSegment(L"Asset/Content/");
		AddFromProjectSegment(L"Asset/");

		for (const std::filesystem::path& Candidate : Candidates)
		{
			std::error_code ErrorCode;
			if (std::filesystem::exists(Candidate, ErrorCode) && std::filesystem::is_regular_file(Candidate, ErrorCode))
			{
				return Candidate;
			}
		}

		// Last-resort fallback for imported FBX/OBJ texture paths that became invalid after
		// moving the project directory: search under Asset/Content by filename.
		const std::wstring FileName = Original.filename().wstring();
		if (!FileName.empty())
		{
			const std::filesystem::path ContentRoot(FPaths::ContentDir());
			std::error_code IterError;
			if (std::filesystem::exists(ContentRoot, IterError))
			{
				for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(ContentRoot, std::filesystem::directory_options::skip_permission_denied, IterError))
				{
					if (IterError)
					{
						break;
					}
					if (!Entry.is_regular_file())
					{
						continue;
					}
					if (Entry.path().filename() == FileName)
					{
						return Entry.path().lexically_normal();
					}
				}
			}
		}

		return Candidates.empty() ? std::filesystem::path() : Candidates.front();
	}

	FString SanitizeAssetName(FString Name)
	{
		if (Name.empty())
		{
			return "Imported";
		}

		for (char& Character : Name)
		{
			const bool bAlphaNum = (Character >= 'a' && Character <= 'z')
				|| (Character >= 'A' && Character <= 'Z')
				|| (Character >= '0' && Character <= '9');
			if (!bAlphaNum && Character != '_' && Character != '-')
			{
				Character = '_';
			}
		}

		return Name;
	}

	bool TryGetTextureWriteTime(const FString& FilePath, std::filesystem::file_time_type& OutWriteTime)
	{
		std::error_code ErrorCode;
		const std::filesystem::path DiskPath(ResolveTexturePathOnDisk(FilePath));
		const std::filesystem::file_time_type WriteTime = std::filesystem::last_write_time(DiskPath, ErrorCode);
		if (ErrorCode)
		{
			return false;
		}

		OutWriteTime = WriteTime;
		return true;
	}

	bool ShouldSkipTextureScanDirectory(const std::filesystem::path& Path)
	{
		const std::wstring Name = Path.filename().wstring();
		return Name == L".git"
			|| Name == L".vs"
			|| Name == L"Bin"
			|| Name == L"Build"
			|| Name == L"Intermediate"
			|| Name == L"Cache";
	}

	bool ShouldIncludeInTextureAssetList(const std::filesystem::path& ProjectRelativePath)
	{
		const std::filesystem::path NormalizedPath = ProjectRelativePath.lexically_normal();
		if (NormalizedPath.empty())
		{
			return false;
		}

		auto It = NormalizedPath.begin();
		if (It == NormalizedPath.end() || *It != L"Asset")
		{
			return true;
		}

		++It;
		if (It == NormalizedPath.end())
		{
			return true;
		}

		if (*It == L"Editor")
		{
			return false;
		}

		if (*It != L"Content")
		{
			return true;
		}

		++It;
		return It == NormalizedPath.end() || *It != L"Font";
	}

	bool IsSupportedTexturePathString(const FString& Path)
	{
		const std::filesystem::path TexturePath(FPaths::ToWide(Path));
		if (UTexture2D::IsSupportedTextureExtension(TexturePath))
		{
			return true;
		}

		std::wstring Ext = TexturePath.extension().wstring();
		std::transform(Ext.begin(), Ext.end(), Ext.begin(), towlower);
		return Ext == L".uasset";
	}

	bool LoadCPUTextureRGBA(const FString& FilePath, uint32& OutWidth, uint32& OutHeight, std::vector<uint8>& OutPixels)
	{
		OutWidth = 0;
		OutHeight = 0;
		OutPixels.clear();
		const std::wstring DiskPath = ResolveTexturePathOnDisk(FilePath);
		if (DiskPath.empty())
		{
			return false;
		}

		IWICImagingFactory* Factory = nullptr;
		HRESULT HR = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&Factory));
		if (FAILED(HR) || !Factory)
		{
			return false;
		}

		IWICBitmapDecoder* Decoder = nullptr;
		HR = Factory->CreateDecoderFromFilename(
			DiskPath.c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnDemand,
			&Decoder);
		if (FAILED(HR) || !Decoder)
		{
			Factory->Release();
			return false;
		}

		IWICBitmapFrameDecode* Frame = nullptr;
		HR = Decoder->GetFrame(0, &Frame);
		if (FAILED(HR) || !Frame)
		{
			Decoder->Release();
			Factory->Release();
			return false;
		}

		HR = Frame->GetSize(&OutWidth, &OutHeight);
		if (FAILED(HR) || OutWidth == 0 || OutHeight == 0)
		{
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		IWICFormatConverter* Converter = nullptr;
		HR = Factory->CreateFormatConverter(&Converter);
		if (FAILED(HR) || !Converter)
		{
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		HR = Converter->Initialize(
			Frame,
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0f,
			WICBitmapPaletteTypeCustom);
		if (FAILED(HR))
		{
			Converter->Release();
			Frame->Release();
			Decoder->Release();
			Factory->Release();
			return false;
		}

		const size_t PixelCount = static_cast<size_t>(OutWidth) * static_cast<size_t>(OutHeight);
		OutPixels.resize(PixelCount * 4ull);
		HR = Converter->CopyPixels(
			nullptr,
			OutWidth * 4u,
			static_cast<UINT>(OutPixels.size()),
			OutPixels.data());

		Converter->Release();
		Frame->Release();
		Decoder->Release();
		Factory->Release();
		return SUCCEEDED(HR);
	}
}

UTexture2D::~UTexture2D()
{
	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
			TrackedTextureMemory = 0;
		}

		SRV->Release();
		SRV = nullptr;
	}

	// 캐시에서 제거
	auto It = TextureCache.find(CacheKeyPath);
	if (It != TextureCache.end() && It->second == this)
	{
		TextureCache.erase(It);
	}
}

void UTexture2D::ReleaseAllGPU()
{
	for (auto& [Path, Texture] : TextureCache)
	{
		if (Texture && Texture->SRV)
		{
			if (Texture->TrackedTextureMemory > 0)
			{
				MemoryStats::SubTextureMemory(Texture->TrackedTextureMemory);
				Texture->TrackedTextureMemory = 0;
			}
			Texture->SRV->Release();
			Texture->SRV = nullptr;
		}
	}
	TextureCache.clear();
}

bool UTexture2D::HasPendingTextureRefresh()
{
	EnsureTextureAssetWatcher();
	return !PendingTextureRefreshPaths.empty();
}

void UTexture2D::RefreshChangedTextures(ID3D11Device* Device)
{
	EnsureTextureAssetWatcher();

	if (!Device)
	{
		return;
	}

	if (PendingTextureRefreshPaths.empty())
	{
		return;
	}

	TSet<FString> ChangedPaths;
	std::swap(ChangedPaths, PendingTextureRefreshPaths);

	for (const FString& ChangedPath : ChangedPaths)
	{
		auto It = TextureCache.find(NormalizeTexturePath(ChangedPath));
		if (It != TextureCache.end() && It->second && It->second->HasSourceFileChanged())
		{
			It->second->LoadInternal(It->first, Device);
		}
	}
}

void UTexture2D::ScanTextureAssets()
{
	EnsureTextureAssetWatcher();
	AvailableTextureFiles.clear();
	TSet<FString> SeenTexturePaths;

	const std::filesystem::path ProjectRoot(FPaths::RootDir());
	const std::filesystem::path AssetRoot(FPaths::AssetDir());
	if (!std::filesystem::exists(AssetRoot))
	{
		return;
	}

	std::error_code ErrorCode;
	std::filesystem::recursive_directory_iterator It(
		AssetRoot,
		std::filesystem::directory_options::skip_permission_denied,
		ErrorCode);
	std::filesystem::recursive_directory_iterator End;

	while (It != End)
	{
		if (ErrorCode)
		{
			ErrorCode.clear();
			It.increment(ErrorCode);
			continue;
		}

		const std::filesystem::directory_entry& Entry = *It;
		if (Entry.is_directory(ErrorCode) && ShouldSkipTextureScanDirectory(Entry.path()))
		{
			It.disable_recursion_pending();
		}
		else if (Entry.is_regular_file(ErrorCode))
		{
			bool bIsTextureListItem = IsSupportedTextureExtension(Entry.path());
			if (!bIsTextureListItem && Entry.path().extension() == L".uasset")
			{
				FAssetFileHeader Header;
				bIsTextureListItem = FAssetFileSerializer::ReadAssetHeader(Entry.path(), Header)
					&& Header.ClassId == EAssetClassId::Texture;
			}

			if (bIsTextureListItem)
			{
				const std::filesystem::path RelativePath = Entry.path().lexically_relative(ProjectRoot).lexically_normal();
				if (!ShouldIncludeInTextureAssetList(RelativePath))
				{
					It.increment(ErrorCode);
					continue;
				}

				const FString NormalizedFullPath = FPaths::NormalizePath(FPaths::ToUtf8(RelativePath.generic_wstring()));
				if (!SeenTexturePaths.insert(NormalizedFullPath).second)
				{
					It.increment(ErrorCode);
					continue;
				}

				FTextureAssetListItem Item;
				Item.DisplayName = FPaths::ToUtf8(Entry.path().filename().wstring());
				Item.FullPath = NormalizedFullPath;
				Item.SourceFolder = FPaths::NormalizePath(FPaths::ToUtf8(RelativePath.parent_path().generic_wstring()));
				AvailableTextureFiles.push_back(std::move(Item));
			}
		}

		It.increment(ErrorCode);
	}

	std::sort(
		AvailableTextureFiles.begin(),
		AvailableTextureFiles.end(),
		[](const FTextureAssetListItem& A, const FTextureAssetListItem& B)
		{
			if (A.SourceFolder != B.SourceFolder)
			{
				return A.SourceFolder < B.SourceFolder;
			}

			if (A.DisplayName != B.DisplayName)
			{
				return A.DisplayName < B.DisplayName;
			}

			return A.FullPath < B.FullPath;
		});

	bTextureAssetListDirty = false;
}

const TArray<FTextureAssetListItem>& UTexture2D::GetAvailableTextureFiles()
{
	EnsureTextureAssetWatcher();
	if (bTextureAssetListDirty)
	{
		ScanTextureAssets();
	}
	return AvailableTextureFiles;
}

bool UTexture2D::IsSupportedTextureExtension(const std::filesystem::path& Path)
{
	std::wstring Ext = Path.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), towlower);

	return Ext == L".png"
		|| Ext == L".jpg"
		|| Ext == L".jpeg"
		|| Ext == L".bmp"
		|| Ext == L".tga"
		|| Ext == L".dds";
}

UTexture2D* UTexture2D::LoadFromAssetFile(const FString& AssetPath, ID3D11Device* Device)
{
	if (AssetPath.empty())
	{
		return nullptr;
	}

	const FString NormalizedAssetPath = NormalizeTexturePath(AssetPath);
	auto CachedIt = TextureCache.find(NormalizedAssetPath);
	if (CachedIt != TextureCache.end())
	{
		if (CachedIt->second && Device && !CachedIt->second->EnsureGPUTexture(Device))
		{
			return nullptr;
		}
		return CachedIt->second;
	}

	FString Error;
	UObject* LoadedObject = FAssetFileSerializer::LoadObjectFromAssetFile(
		std::filesystem::path(FPaths::ResolvePathToDisk(NormalizedAssetPath)),
		&Error);
	UTexture2D* LoadedTextureAsset = Cast<UTexture2D>(LoadedObject);
	if (!LoadedTextureAsset)
	{
		if (LoadedObject)
		{
			UObjectManager::Get().DestroyObject(LoadedObject);
		}
		return nullptr;
	}

	LoadedTextureAsset->AssetFilePath = NormalizedAssetPath;
	LoadedTextureAsset->CacheKeyPath = NormalizedAssetPath;

	std::filesystem::file_time_type SourceWriteTime{};
	LoadedTextureAsset->bHasSourceFileWriteTime = TryGetTextureWriteTime(LoadedTextureAsset->SourceFilePath, SourceWriteTime);
	if (LoadedTextureAsset->bHasSourceFileWriteTime)
	{
		LoadedTextureAsset->SourceFileWriteTime = SourceWriteTime;
	}

	if (Device && !LoadedTextureAsset->EnsureGPUTexture(Device))
	{
		UObjectManager::Get().DestroyObject(LoadedTextureAsset);
		return nullptr;
	}

	TextureCache[NormalizedAssetPath] = LoadedTextureAsset;
	return LoadedTextureAsset;
}

UTexture2D* UTexture2D::LoadFromFile(const FString& FilePath, ID3D11Device* Device)
{
	if (FilePath.empty())
	{
		return nullptr;
	}

	const FString NormalizedPath = NormalizeTexturePath(FilePath);
	const std::wstring Extension = std::filesystem::path(FPaths::ToWide(NormalizedPath)).extension().wstring();
	if (_wcsicmp(Extension.c_str(), L".uasset") == 0)
	{
		return LoadFromAssetFile(NormalizedPath, Device);
	}

	// 캐시 히트
	auto It = TextureCache.find(NormalizedPath);
	if (It != TextureCache.end())
	{
		if (It->second && It->second->HasSourceFileChanged())
		{
			It->second->LoadInternal(NormalizedPath, Device);
		}
		else if (It->second && Device)
		{
			It->second->EnsureGPUTexture(Device);
		}
		return It->second;
	}

	// 새 UTexture2D 생성
	UTexture2D* Texture = UObjectManager::Get().CreateObject<UTexture2D>();
	if (!Texture->LoadInternal(FilePath, Device))
	{
		UObjectManager::Get().DestroyObject(Texture);
		return nullptr;
	}

	Texture->AssetFilePath.clear();
	Texture->CacheKeyPath = NormalizedPath;
	TextureCache[NormalizedPath] = Texture;
	return Texture;
}

UTexture2D* UTexture2D::LoadFromCached(const FString& FilePath)
{
	if (FilePath.empty()) return nullptr;

	auto It = TextureCache.find(NormalizeTexturePath(FilePath));
	if (It != TextureCache.end())
	{
		return It->second;
	}

	return nullptr;
}

bool UTexture2D::LoadInternal(const FString& FilePath, ID3D11Device* Device)
{
	const FString NormalizedPath = NormalizeTexturePath(FilePath);
	const std::wstring WidePath = ResolveTexturePathOnDisk(NormalizedPath);
	if (WidePath.empty() || !std::filesystem::exists(WidePath))
	{
		UE_LOG_CATEGORY(Texture, Error, "Failed to load texture: %s", FilePath.c_str());
		return false;
	}

	uint32 NewWidth = 0;
	uint32 NewHeight = 0;
	std::vector<uint8> NewCPUTextureRGBA;
	LoadCPUTextureRGBA(NormalizedPath, NewWidth, NewHeight, NewCPUTextureRGBA);

	std::filesystem::file_time_type NewWriteTime{};
	const bool bHasNewWriteTime = TryGetTextureWriteTime(NormalizedPath, NewWriteTime);

	if (!Device)
	{
		if (NewCPUTextureRGBA.empty() || NewWidth == 0 || NewHeight == 0)
		{
			UE_LOG_CATEGORY(Texture, Error, "Failed to load texture pixels without device: %s", FilePath.c_str());
			return false;
		}

		if (SRV)
		{
			if (TrackedTextureMemory > 0)
			{
				MemoryStats::SubTextureMemory(TrackedTextureMemory);
				TrackedTextureMemory = 0;
			}
			SRV->Release();
			SRV = nullptr;
		}

		Width = NewWidth;
		Height = NewHeight;
		CPUTextureRGBA = std::move(NewCPUTextureRGBA);
		SourceFilePath = NormalizedPath;
		SourceFileWriteTime = NewWriteTime;
		bHasSourceFileWriteTime = bHasNewWriteTime;
		return true;
	}

	const std::filesystem::path ExtensionPath = std::filesystem::path(WidePath).extension();
	FString Extension = FPaths::ToUtf8(ExtensionPath.generic_wstring());
	AsciiUtils::ToLowerInPlace(Extension);

	ID3D11Resource* Resource = nullptr;
	ID3D11ShaderResourceView* NewSRV = nullptr;
	HRESULT hr = S_OK;
	if (Extension == ".dds")
	{
		hr = DirectX::CreateDDSTextureFromFileEx(
			Device, WidePath.c_str(),
			0,                           // maxsize
			D3D11_USAGE_DEFAULT,         // usage
			D3D11_BIND_SHADER_RESOURCE,  // bindFlags
			0,                           // cpuAccessFlags
			0,                           // miscFlags
			DirectX::DDS_LOADER_DEFAULT,
			&Resource, &NewSRV);
	}
	else
	{
		hr = DirectX::CreateWICTextureFromFileEx(
			Device, WidePath.c_str(),
			0,                                    // maxsize
			D3D11_USAGE_DEFAULT,                  // usage
			D3D11_BIND_SHADER_RESOURCE,           // bindFlags
			0,                                    // cpuAccessFlags
			0,                                    // miscFlags
			DirectX::WIC_LOADER_IGNORE_SRGB,      // sRGB 메타데이터 무시 → UNORM 포맷 강제
			&Resource, &NewSRV);
	}

	if (FAILED(hr) || !NewSRV)
	{
		UE_LOG_CATEGORY(Texture, Error, "Failed to load texture: %s", FilePath.c_str());
		if (Resource)
		{
			Resource->Release();
		}
		if (NewSRV)
		{
			NewSRV->Release();
		}
		return false;
	}

	uint64 NewTrackedTextureMemory = 0;

	// 텍스처 크기 추출
	if (Resource)
	{
		NewTrackedTextureMemory = MemoryStats::CalculateTextureMemory(Resource);

		ID3D11Texture2D* Tex2D = nullptr;
		if (SUCCEEDED(Resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&Tex2D)))
		{
			D3D11_TEXTURE2D_DESC Desc;
			Tex2D->GetDesc(&Desc);
			NewWidth = Desc.Width;
			NewHeight = Desc.Height;
			Tex2D->Release();
		}
		Resource->Release();
	}

	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
			TrackedTextureMemory = 0;
		}
		SRV->Release();
		SRV = nullptr;
	}

	SRV = NewSRV;
	Width = NewWidth;
	Height = NewHeight;
	TrackedTextureMemory = NewTrackedTextureMemory;
	CPUTextureRGBA = std::move(NewCPUTextureRGBA);
	SourceFilePath = NormalizedPath;
	SourceFileWriteTime = NewWriteTime;
	bHasSourceFileWriteTime = bHasNewWriteTime;

	if (TrackedTextureMemory > 0)
	{
		MemoryStats::AddTextureMemory(TrackedTextureMemory);
	}

	return true;
}

bool UTexture2D::EnsureGPUTexture(ID3D11Device* Device)
{
	if (SRV)
	{
		return true;
	}

	if (!Device)
	{
		return !CPUTextureRGBA.empty();
	}

	if (CreateSRVFromStoredPixels(Device))
	{
		return true;
	}

	if (!SourceFilePath.empty())
	{
		return LoadInternal(SourceFilePath, Device);
	}

	return false;
}

bool UTexture2D::CreateSRVFromStoredPixels(ID3D11Device* Device)
{
	if (!Device || Width == 0 || Height == 0 || CPUTextureRGBA.empty())
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = CPUTextureRGBA.data();
	InitialData.SysMemPitch = Width * 4u;

	ID3D11Texture2D* TextureResource = nullptr;
	HRESULT HR = Device->CreateTexture2D(&Desc, &InitialData, &TextureResource);
	if (FAILED(HR) || !TextureResource)
	{
		return false;
	}

	ID3D11ShaderResourceView* NewSRV = nullptr;
	HR = Device->CreateShaderResourceView(TextureResource, nullptr, &NewSRV);
	const uint64 NewTrackedTextureMemory = MemoryStats::CalculateTextureMemory(TextureResource);
	TextureResource->Release();

	if (FAILED(HR) || !NewSRV)
	{
		if (NewSRV)
		{
			NewSRV->Release();
		}
		return false;
	}

	if (SRV)
	{
		if (TrackedTextureMemory > 0)
		{
			MemoryStats::SubTextureMemory(TrackedTextureMemory);
		}
		SRV->Release();
	}

	SRV = NewSRV;
	TrackedTextureMemory = NewTrackedTextureMemory;
	if (TrackedTextureMemory > 0)
	{
		MemoryStats::AddTextureMemory(TrackedTextureMemory);
	}

	return true;
}

bool UTexture2D::HasSourceFileChanged() const
{
	if (!AssetFilePath.empty() || SourceFilePath.empty())
	{
		return false;
	}

	std::filesystem::file_time_type CurrentWriteTime{};
	if (!TryGetTextureWriteTime(SourceFilePath, CurrentWriteTime))
	{
		return false;
	}

	if (!bHasSourceFileWriteTime)
	{
		return true;
	}

	return CurrentWriteTime != SourceFileWriteTime;
}

FString UTexture2D::ImportTextureAsset(const FString& SourcePath)
{
	const std::filesystem::path SourceDiskPathW = FindTextureSourceOnDisk(SourcePath);
	const FString NormalizedSourcePath = NormalizeTexturePath(FPaths::ToUtf8(SourceDiskPathW.generic_wstring()));
	const std::wstring SourceDiskPath = SourceDiskPathW.wstring();
	if (SourceDiskPath.empty() || !std::filesystem::exists(SourceDiskPathW))
	{
		UE_LOG_CATEGORY(Texture, Error, "Texture import source not found: source=%s resolved=%s", SourcePath.c_str(), FPaths::ToUtf8(SourceDiskPath).c_str());
		NotifyTextureImport("Texture import failed: source not found - " + GetFileNameUtf8(SourcePath), ENotificationType::Error, 5.0f);
		return {};
	}

	const FString SourceStem = SanitizeAssetName(FPaths::ToUtf8(SourceDiskPathW.stem().wstring()));
	const std::filesystem::path TextureDirectory = std::filesystem::path(FPaths::TexturesDir()).lexically_normal();
	std::error_code DirectoryError;
	std::filesystem::create_directories(TextureDirectory, DirectoryError);
	if (DirectoryError)
	{
		UE_LOG_CATEGORY(Texture, Error, "Failed to create texture asset directory: %s (%s)", FPaths::ToUtf8(TextureDirectory.generic_wstring()).c_str(), DirectoryError.message().c_str());
		NotifyTextureImport("Texture import failed: cannot create Asset/Content/Textures", ENotificationType::Error, 5.0f);
		return {};
	}

	const std::filesystem::path AssetDiskPath = TextureDirectory / (L"T_" + FPaths::ToWide(SourceStem) + L".uasset");
	std::error_code RelativeError;
	std::filesystem::path RelativeAssetPath = std::filesystem::relative(AssetDiskPath, std::filesystem::path(FPaths::RootDir()), RelativeError);
	if (RelativeError || RelativeAssetPath.empty())
	{
		RelativeAssetPath = AssetDiskPath;
	}
	const FString AssetPath = NormalizeTexturePath(FPaths::ToUtf8(RelativeAssetPath.generic_wstring()));

	NotifyTextureImport("Texture import: " + GetFileNameUtf8(NormalizedSourcePath) + " -> " + AssetPath, ENotificationType::Info, 2.5f);
	UE_LOG_CATEGORY(Texture, Info, "Texture import begin: source=%s resolved=%s target=%s", NormalizedSourcePath.c_str(), FPaths::ToUtf8(SourceDiskPath).c_str(), AssetPath.c_str());

	UTexture2D* Texture = nullptr;
	auto CachedIt = TextureCache.find(AssetPath);
	const bool bCreatedNewTexture = CachedIt == TextureCache.end();
	if (CachedIt != TextureCache.end())
	{
		Texture = CachedIt->second;
	}
	else
	{
		Texture = UObjectManager::Get().CreateObject<UTexture2D>();
	}

	if (!Texture)
	{
		NotifyTextureImport("Texture import failed: cannot create UTexture2D", ENotificationType::Error, 5.0f);
		return {};
	}

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Texture->LoadInternal(NormalizedSourcePath, Device))
	{
		if (bCreatedNewTexture)
		{
			UObjectManager::Get().DestroyObject(Texture);
		}
		UE_LOG_CATEGORY(Texture, Error, "Texture import decode/load failed: source=%s target=%s", NormalizedSourcePath.c_str(), AssetPath.c_str());
		NotifyTextureImport("Texture import failed: decode/load failed - " + GetFileNameUtf8(NormalizedSourcePath), ENotificationType::Error, 5.0f);
		return {};
	}

	if (Texture->CPUTextureRGBA.empty() || Texture->Width == 0 || Texture->Height == 0)
	{
		if (bCreatedNewTexture)
		{
			UObjectManager::Get().DestroyObject(Texture);
		}
		UE_LOG_CATEGORY(Texture, Error, "Texture import produced no CPU pixel data: source=%s target=%s", NormalizedSourcePath.c_str(), AssetPath.c_str());
		NotifyTextureImport("Texture import failed: no pixel data - " + GetFileNameUtf8(NormalizedSourcePath), ENotificationType::Error, 5.0f);
		return {};
	}

	Texture->SetFName(FName("T_" + SourceStem));
	Texture->AssetFilePath = AssetPath;
	Texture->CacheKeyPath = AssetPath;
	Texture->SourceFilePath = NormalizedSourcePath;
	TextureCache[AssetPath] = Texture;

	FString Error;
	if (!FAssetFileSerializer::SaveObjectToAssetFile(AssetDiskPath, Texture, &Error))
	{
		if (bCreatedNewTexture)
		{
			TextureCache.erase(AssetPath);
			UObjectManager::Get().DestroyObject(Texture);
		}
		UE_LOG_CATEGORY(Texture, Error, "Failed to save texture asset: %s (%s)", AssetPath.c_str(), Error.c_str());
		NotifyTextureImport(Error.empty() ? "Texture import failed: could not save .uasset" : Error, ENotificationType::Error, 5.0f);
		return {};
	}

	MarkTextureAssetListDirty();
	QueueTextureRefresh(AssetPath);
	UE_LOG_CATEGORY(Texture, Info, "Imported texture asset: source=%s target=%s size=%ux%u bytes=%zu", NormalizedSourcePath.c_str(), AssetPath.c_str(), Texture->Width, Texture->Height, Texture->CPUTextureRGBA.size());
	NotifyTextureImport("Saved texture asset: " + AssetPath, ENotificationType::Success, 3.0f);
	return AssetPath;
}

void UTexture2D::EnsureTextureAssetWatcher()
{
	if (bTextureAssetWatcherInitialized)
	{
		return;
	}

	bTextureAssetWatcherInitialized = true;
	TextureAssetWatchID = FDirectoryWatcher::Get().Watch(FPaths::AssetDir(), "Asset/");
	if (TextureAssetWatchID == 0)
	{
		return;
	}

	TextureAssetWatchSub = FDirectoryWatcher::Get().Subscribe(
		TextureAssetWatchID,
		[](const TSet<FString>& ChangedPaths)
		{
			for (const FString& Path : ChangedPaths)
			{
				if (IsSupportedTexturePathString(Path))
				{
					MarkTextureAssetListDirty();
					QueueTextureRefresh(Path);
				}
			}
		});
}

void UTexture2D::MarkTextureAssetListDirty()
{
	bTextureAssetListDirty = true;
}

void UTexture2D::QueueTextureRefresh(const FString& TexturePath)
{
	if (TexturePath.empty())
	{
		return;
	}

	PendingTextureRefreshPaths.insert(NormalizeTexturePath(TexturePath));
}

void UTexture2D::Serialize(FArchive& Ar)
{
	Ar << SourceFilePath;
	Ar << Width;
	Ar << Height;

	const uint32 AssetVersion = FAssetFileSerializer::GetCurrentAssetSerializationVersion();
	if (AssetVersion >= 4)
	{
		uint32 PixelByteCount = static_cast<uint32>(CPUTextureRGBA.size());
		Ar << PixelByteCount;

		if (Ar.IsLoading())
		{
			CPUTextureRGBA.resize(PixelByteCount);
		}

		if (PixelByteCount > 0)
		{
			Ar.Serialize(CPUTextureRGBA.data(), PixelByteCount);
		}
	}
	else if (Ar.IsLoading())
	{
		CPUTextureRGBA.clear();
	}

	if (Ar.IsLoading())
	{
		AssetFilePath.clear();
		CacheKeyPath.clear();
		SourceFileWriteTime = {};
		bHasSourceFileWriteTime = TryGetTextureWriteTime(SourceFilePath, SourceFileWriteTime);
	}
}

bool UTexture2D::SampleAlpha(float U, float V, float& OutAlpha) const
{
	OutAlpha = 1.0f;

	if (CPUTextureRGBA.empty() || Width == 0 || Height == 0)
	{
		return false;
	}

	const float ClampedU = std::clamp(U, 0.0f, 1.0f);
	const float ClampedV = std::clamp(V, 0.0f, 1.0f);
	const uint32 X = std::min<uint32>(static_cast<uint32>(ClampedU * static_cast<float>(Width - 1)), Width - 1);
	const uint32 Y = std::min<uint32>(static_cast<uint32>(ClampedV * static_cast<float>(Height - 1)), Height - 1);
	const size_t PixelIndex = (static_cast<size_t>(Y) * static_cast<size_t>(Width) + static_cast<size_t>(X)) * 4ull;

	if (PixelIndex + 3ull >= CPUTextureRGBA.size())
	{
		return false;
	}

	OutAlpha = static_cast<float>(CPUTextureRGBA[PixelIndex + 3ull]) / 255.0f;
	return true;
}
