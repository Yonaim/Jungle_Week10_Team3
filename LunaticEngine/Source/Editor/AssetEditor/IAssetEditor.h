#pragma once

#include <filesystem>

#include "Core/CoreTypes.h"

#include "ImGui/imgui.h"

class UObject;
class UEditorEngine;
class FRenderer;
class FEditorViewportClient;

/**
 * 모든 에셋 에디터의 공통 인터페이스.
 *
 * 이 인터페이스는 Window나 Tab 자체가 아니라, "탭/패널 내부에 그려질 편집기 내용"을 의미한다.
 * 예:
 * - FCameraModifierStackEditor
 * - FSkeletalMeshEditor
 *
 * 기본 에디터는 하나의 도킹 가능한 패널 안에 RenderContent()를 그린다.
 * SkeletalMeshEditor처럼 Toolbar / Preview / Skeleton Tree / Details를 각각 별도 패널로 나누고 싶으면
 * UsesExternalPanels()에서 true를 반환하고 RenderPanels()를 직접 구현한다.
 */
class IAssetEditor
{
  public:
    virtual ~IAssetEditor() = default;

    virtual void Initialize(UEditorEngine *InEditorEngine, FRenderer *InRenderer) = 0;

    virtual bool OpenAsset(UObject *Asset, const std::filesystem::path &AssetPath) = 0;
    virtual void Close() = 0;
    virtual bool Save() = 0;

    virtual void Tick(float DeltaTime) {}

    /** 단일 패널형 에디터의 본문 렌더링. */
    virtual void RenderContent(float DeltaTime) = 0;

    virtual bool UsesExternalPanels() const { return false; }
    virtual void RenderPanels(float DeltaTime, ImGuiID DockspaceId)
    {
        (void)DockspaceId;
        RenderContent(DeltaTime);
    }

    // 공통 메뉴바에서 활성 에디터가 자기 전용 메뉴를 추가할 수 있도록 둔 hook.
    virtual void BuildFileMenu() {}
    virtual void BuildEditMenu() {}
    virtual void BuildWindowMenu() {}
    virtual void BuildCustomMenus() {}

    virtual bool IsDirty() const = 0;
    virtual bool IsCapturingInput() const = 0;

    /** Asset Editor 내부에 있는 렌더 가능한 Preview Viewport들을 수집한다. */
    virtual void CollectViewportClients(TArray<FEditorViewportClient *> &OutClients) { (void)OutClients; }
    virtual FEditorViewportClient *GetActiveViewportClient() { return nullptr; }

    virtual const char *GetEditorName() const = 0;
    virtual const std::filesystem::path &GetAssetPath() const = 0;
};
