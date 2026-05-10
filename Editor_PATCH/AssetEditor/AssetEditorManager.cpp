#include "AssetEditor/AssetEditorManager.h"

#include "AssetEditor/CameraModifierStack/CameraModifierStackEditor.h"
#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"
#include "AssetEditor/Window/AssetEditorWindow.h"
#include "Common/File/EditorFileUtils.h"
#include "Core/Notification.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "Object/Object.h"
#include "Platform/Paths.h"

#include <algorithm>
#include <cwctype>


namespace
{
std::wstring ToLowerExtension(const std::filesystem::path &Path)
{
    std::wstring Extension = Path.extension().wstring();
    std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](wchar_t Ch) {
        return static_cast<wchar_t>(std::towlower(Ch));
    });
    return Extension;
}
} // namespace

void FAssetEditorManager::Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer)
{
    EditorEngine = InEditorEngine;
    Renderer = InRenderer;
}

void FAssetEditorManager::Shutdown()
{
    AssetEditorWindow.Shutdown();
    Renderer = nullptr;
    EditorEngine = nullptr;
}

void FAssetEditorManager::Tick(float DeltaTime)
{
    AssetEditorWindow.Tick(DeltaTime);
}

void FAssetEditorManager::RenderContent(float DeltaTime)
{
    AssetEditorWindow.RenderContent(DeltaTime);
}

bool FAssetEditorManager::OpenAssetFromPath(const std::filesystem::path &AssetPath)
{
    const std::wstring Extension = ToLowerExtension(AssetPath);
    if (Extension != L".uasset")
    {
        return OpenSourceFileFromPath(AssetPath);
    }

    UObject *LoadedAsset = nullptr;
    FString Error;
    LoadedAsset = FAssetFileSerializer::LoadAssetFromFile(AssetPath, &Error);

    if (!LoadedAsset)
    {
        FNotificationManager::Get().AddNotification(Error.empty() ? "Failed to load asset." : Error, ENotificationType::Error, 5.0f);
        return false;
    }

    if (OpenLoadedAsset(LoadedAsset, AssetPath))
    {
        return true;
    }

    UObjectManager::Get().DestroyObject(LoadedAsset);
    return false;
}

bool FAssetEditorManager::OpenSourceFileFromPath(const std::filesystem::path &SourcePath)
{
    const std::wstring Extension = ToLowerExtension(SourcePath);

    if (Extension == L".uasset")
    {
        return OpenAssetFromPath(SourcePath);
    }

    if (Extension == L".fbx")
    {
        return OpenFbxForPreview(SourcePath);
    }

    FNotificationManager::Get().AddNotification("Unsupported source file type.", ENotificationType::Warning, 3.0f);
    return false;
}

bool FAssetEditorManager::OpenLoadedAsset(UObject *Asset, const std::filesystem::path &AssetPath)
{
    if (!Asset)
    {
        return false;
    }

    const std::filesystem::path NormalizedPath = AssetPath.lexically_normal();

    if (AssetEditorWindow.IsOpen() && AssetEditorWindow.ActivateTabByAssetPath(NormalizedPath))
    {
        AssetEditorWindow.Show();
        return true;
    }

    std::unique_ptr<IAssetEditor> Editor = CreateEditorForAsset(Asset);
    if (!Editor)
    {
        FNotificationManager::Get().AddNotification("No editor registered for this asset.", ENotificationType::Info, 3.0f);
        return false;
    }

    Editor->Initialize(EditorEngine, Renderer);
    if (!Editor->OpenAsset(Asset, NormalizedPath))
    {
        Editor->Close();
        return false;
    }

    AssetEditorWindow.Initialize(EditorEngine, this);

    AssetEditorWindow.OpenEditorTab(std::move(Editor));
    AssetEditorWindow.Show();
    return true;
}

bool FAssetEditorManager::OpenAssetWithDialog(void *OwnerWindowHandle)
{
    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"Asset Files (*.uasset)\0*.uasset\0All Files (*.*)\0*.*\0",
        .Title = L"Open Asset",
        .InitialDirectory = FPaths::AssetDir().c_str(),
        .OwnerWindowHandle = OwnerWindowHandle,
        .bFileMustExist = true,
        .bPathMustExist = true,
        .bPromptOverwrite = false,
        .bReturnRelativeToProjectRoot = false,
    });

    if (SelectedPath.empty())
    {
        return false;
    }

    return OpenAssetFromPath(std::filesystem::path(FPaths::ToWide(SelectedPath)));
}

bool FAssetEditorManager::OpenFbxWithDialog(void *OwnerWindowHandle)
{
    const FString SelectedPath = FEditorFileUtils::OpenFileDialog({
        .Filter = L"FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0",
        .Title = L"Open FBX",
        .InitialDirectory = FPaths::AssetDir().c_str(),
        .OwnerWindowHandle = OwnerWindowHandle,
        .bFileMustExist = true,
        .bPathMustExist = true,
        .bPromptOverwrite = false,
        .bReturnRelativeToProjectRoot = false,
    });

    if (SelectedPath.empty())
    {
        return false;
    }

    return OpenSourceFileFromPath(std::filesystem::path(FPaths::ToWide(SelectedPath)));
}

bool FAssetEditorManager::OpenFbxForPreview(const std::filesystem::path &FbxPath)
{
    const std::filesystem::path NormalizedPath = FbxPath.lexically_normal();
    if (!std::filesystem::exists(NormalizedPath))
    {
        FNotificationManager::Get().AddNotification("FBX file not found.", ENotificationType::Error, 3.0f);
        return false;
    }

    // 성원희 담당 영역 Placeholder:
    // FBX SDK 기반 importer가 완성되면 아래 dummy 생성 경로를 실제 importer로 교체한다.
    // 예) ImportedMesh = FFbxSkeletalMeshImporter::ImportSkeletalMeshForPreview(NormalizedPath);
    USkeletalMesh *ImportedMesh = CreateDummySkeletalMeshForEditorPreview(NormalizedPath);
    if (!ImportedMesh)
    {
        FNotificationManager::Get().AddNotification("Failed to create skeletal mesh preview from FBX.", ENotificationType::Error, 3.0f);
        return false;
    }

    return OpenLoadedAsset(ImportedMesh, NormalizedPath);
}

bool FAssetEditorManager::ShowAssetEditorWindow()
{
    AssetEditorWindow.Initialize(EditorEngine, this);
    AssetEditorWindow.Show();
    return true;
}

bool FAssetEditorManager::CreateCameraModifierStackAsset()
{
    std::unique_ptr<FCameraModifierStackEditor> Editor = std::make_unique<FCameraModifierStackEditor>();
    Editor->Initialize(EditorEngine, Renderer);
    if (!Editor->CreateCameraShakeAsset())
    {
        Editor->Close();
        return false;
    }

    AssetEditorWindow.Initialize(EditorEngine, this);

    AssetEditorWindow.OpenEditorTab(std::move(Editor));
    AssetEditorWindow.Show();
    return true;
}

bool FAssetEditorManager::SaveActiveEditor()
{
    return AssetEditorWindow.SaveActiveTab();
}

void FAssetEditorManager::CloseActiveEditor()
{
    AssetEditorWindow.CloseActiveTab();
}

bool FAssetEditorManager::IsCapturingInput() const
{
    return AssetEditorWindow.IsCapturingInput();
}

USkeletalMesh *FAssetEditorManager::CreateDummySkeletalMeshForEditorPreview(const std::filesystem::path &SourcePath) const
{
    USkeletalMesh *Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
    if (!Mesh)
    {
        return nullptr;
    }

    const FString MeshName = FPaths::ToUtf8(SourcePath.stem().wstring());
    Mesh->SetFName(FName(MeshName.empty() ? FString("PreviewSkeletalMesh") : MeshName));

    // 성원희 담당 영역 Placeholder:
    // 실제 FBX Importer가 완성되면 여기서 dummy data 대신
    // FBX에서 파싱한 Skeleton / LOD / Section / SkinWeight 데이터를 채운다.
    //
    // 김연하 담당 영역:
    // SkeletalMeshEditor는 이 USkeletalMesh를 받아 Details / Preview / Skeleton Tree에 표시한다.
    return Mesh;
}

std::unique_ptr<IAssetEditor> FAssetEditorManager::CreateEditorForAsset(UObject *Asset) const
{
    if (Cast<UCameraModifierStackAssetData>(Asset))
    {
        return std::make_unique<FCameraModifierStackEditor>();
    }

    if (Cast<USkeletalMesh>(Asset))
    {
        return std::make_unique<FSkeletalMeshEditor>();
    }

    return nullptr;
}
