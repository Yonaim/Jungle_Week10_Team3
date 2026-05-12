#include "AssetEditor/AssetEditorManager.h"

#include "AssetEditor/CameraModifierStack/CameraModifierStackEditor.h"
#include "AssetEditor/IAssetEditor.h"
#include "AssetEditor/SkeletalMesh/SkeletalMeshEditor.h"
#include "AssetEditor/Window/AssetEditorWindow.h"
#include "Common/File/EditorFileUtils.h"
#include "Core/Notification.h"
#include "Engine/Asset/AssetData.h"
#include "Engine/Asset/AssetFileSerializer.h"
#include "Engine/Mesh/FbxImporter.h"
#include "Engine/Mesh/SkeletalMesh.h"
#include "EditorEngine.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

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

bool IsSkeletalMeshCachePath(const std::filesystem::path &Path)
{
    if (ToLowerExtension(Path) != L".bin")
    {
        return false;
    }

    const std::wstring Stem = Path.stem().wstring();
    constexpr wchar_t SkeletalSuffix[] = L"_Skeletal";
    constexpr size_t  SuffixLength = (sizeof(SkeletalSuffix) / sizeof(wchar_t)) - 1;
    return Stem.size() >= SuffixLength && Stem.compare(Stem.size() - SuffixLength, SuffixLength, SkeletalSuffix) == 0;
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

void FAssetEditorManager::RenderContent(float DeltaTime, ImGuiID DockspaceId)
{
    AssetEditorWindow.RenderContent(DeltaTime, DockspaceId);
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

    if (IsSkeletalMeshCachePath(SourcePath))
    {
        UObject *LoadedAsset = CreateSkeletalMeshForEditorPreview(SourcePath);
        if (!LoadedAsset)
        {
            FNotificationManager::Get().AddNotification("Failed to load skeletal mesh cache.", ENotificationType::Error, 3.0f);
            return false;
        }

        const bool bOpened = OpenLoadedAsset(LoadedAsset, SourcePath);
        if (!bOpened)
        {
            UObjectManager::Get().DestroyObject(LoadedAsset);
        }

        if (bOpened && EditorEngine)
        {
            EditorEngine->HideLevelEditorUIForAssetEditor();
        }

        return bOpened;
    }

    FNotificationManager::Get().AddNotification("Unsupported source file type.", ENotificationType::Info, 3.0f);
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
        if (EditorEngine)
        {
            EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
        }
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
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
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

    USkeletalMesh *ImportedMesh = CreateSkeletalMeshForEditorPreview(NormalizedPath);
    if (!ImportedMesh)
    {
        FNotificationManager::Get().AddNotification("Failed to create skeletal mesh preview from FBX.", ENotificationType::Error, 3.0f);
        return false;
    }

    const bool bOpened = OpenLoadedAsset(ImportedMesh, NormalizedPath);
    if (!bOpened)
    {
        UObjectManager::Get().DestroyObject(ImportedMesh);
        return false;
    }

    if (EditorEngine)
    {
        // 임시 UX:
        // FBX를 열면 Level Editor의 기존 패널/뷰포트는 숨기고,
        // SkeletalMesh Viewer 관련 Asset Editor 패널만 보이도록 전환한다.
        // 나중에 별도 Asset Editor Workspace / Window를 다시 도입하면 이 호출은 제거한다.
        EditorEngine->HideLevelEditorUIForAssetEditor();
    }

    return true;
}

bool FAssetEditorManager::ShowAssetEditorWindow()
{
    AssetEditorWindow.Initialize(EditorEngine, this);
    AssetEditorWindow.Show();
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
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
    if (EditorEngine)
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::AssetEditor);
    }
    return true;
}

bool FAssetEditorManager::SaveActiveEditor()
{
    return AssetEditorWindow.SaveActiveTab();
}

void FAssetEditorManager::CloseActiveEditor()
{
    AssetEditorWindow.CloseActiveTab();
    if (EditorEngine && !AssetEditorWindow.IsOpen())
    {
        EditorEngine->SetActiveEditorContext(EEditorContextType::LevelEditor);
    }
}

bool FAssetEditorManager::IsCapturingInput() const
{
    return AssetEditorWindow.IsCapturingInput();
}


FEditorViewportClient *FAssetEditorManager::GetActiveViewportClient() const
{
    return AssetEditorWindow.GetActiveViewportClient();
}

void FAssetEditorManager::CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) const
{
    AssetEditorWindow.CollectViewportClients(OutClients);
}

USkeletalMesh *FAssetEditorManager::CreateSkeletalMeshForEditorPreview(const std::filesystem::path &SourcePath) const
{
    const std::filesystem::path NormalizedPath = SourcePath.lexically_normal();
    const std::wstring Extension = ToLowerExtension(NormalizedPath);

    USkeletalMesh *Mesh = UObjectManager::Get().CreateObject<USkeletalMesh>();
    if (!Mesh)
    {
        return nullptr;
    }

    const FString MeshName = FPaths::ToUtf8(NormalizedPath.stem().wstring());
    Mesh->SetFName(FName(MeshName.empty() ? FString("PreviewSkeletalMesh") : MeshName));

    if (Extension == L".fbx")
    {
        FSkeletalMesh *ImportedAsset = new FSkeletalMesh();
        TArray<FStaticMaterial> ImportedMaterials;
        const FString RelativePath = FPaths::ToUtf8(NormalizedPath.lexically_relative(FPaths::RootDir()).generic_wstring());
        if (!FFbxSkeletalMeshImporter::Import(RelativePath, *ImportedAsset, ImportedMaterials))
        {
            delete ImportedAsset;
            UObjectManager::Get().DestroyObject(Mesh);
            return nullptr;
        }

        Mesh->SetStaticMaterials(std::move(ImportedMaterials));
        Mesh->SetSkeletalMeshAsset(ImportedAsset);
        return Mesh;
    }

    if (Extension == L".bin")
    {
        const FString BinPath = FPaths::ToUtf8(NormalizedPath.generic_wstring());
        FWindowsBinReader Reader(BinPath);
        if (!Reader.IsValid())
        {
            UObjectManager::Get().DestroyObject(Mesh);
            return nullptr;
        }

        Mesh->Serialize(Reader);
        if (Mesh->IsValid())
        {
            return Mesh;
        }

        UObjectManager::Get().DestroyObject(Mesh);
        return nullptr;
    }

    // 성원희 담당 영역 Placeholder:
    // 실제 FBX Importer가 완성되면 여기서 dummy data 대신
    // FBX에서 파싱한 Skeleton / LOD / Section / SkinWeight 데이터를 채운다.
    //
    // 김연하 담당 영역:
    // SkeletalMeshEditor는 이 USkeletalMesh를 받아 Details / Preview / Skeleton Tree에 표시한다.
    UObjectManager::Get().DestroyObject(Mesh);
    return nullptr;
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
