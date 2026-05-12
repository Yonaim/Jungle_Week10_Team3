#include "EditorRenderPipeline.h"
#include "Component/CameraComponent.h"
#include "Component/Light/LightComponentBase.h"
#include "Core/ProjectSettings.h"
#include "EditorEngine.h"
#include "LevelEditor/Viewport/LevelEditorViewportClient.h"
#include "Common/Viewport/EditorViewportClient.h"
#include "Common/Viewport/EditorViewportCamera.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Engine/Render/Types/ForwardLightData.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Profiling/GPUProfiler.h"
#include "Profiling/Stats.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Scene/FScene.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/Viewport.h"


FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine *InEditor, FRenderer &InRenderer)
    : Editor(InEditor), CachedDevice(InRenderer.GetFD3DDevice().GetDevice())
{
}

FEditorRenderPipeline::~FEditorRenderPipeline()
{
}

void FEditorRenderPipeline::OnSceneCleared()
{
    for (auto &[VC, Occlusion] : GPUOcclusionMap)
    {
        Occlusion->InvalidateResults();
    }

    for (FLevelEditorViewportClient *VC : Editor->GetLevelViewportClients())
    {
        VC->ClearLightViewOverride();
    }
}

FGPUOcclusionCulling &FEditorRenderPipeline::GetOcclusionForViewport(FLevelEditorViewportClient *VC)
{
    auto it = GPUOcclusionMap.find(VC);
    if (it != GPUOcclusionMap.end())
        return *it->second;

    auto ptr = std::make_unique<FGPUOcclusionCulling>();
    ptr->Initialize(CachedDevice);
    auto &ref = *ptr;
    GPUOcclusionMap.emplace(VC, std::move(ptr));
    return ref;
}

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer &Renderer)
{
#if STATS
    FStatManager::Get().TakeSnapshot();
    FGPUProfiler::Get().TakeSnapshot();
    FGPUProfiler::Get().BeginFrame();
#endif

    // 이전 프레임 시각화 데이터 readback + 디버그 라인 제출
    Renderer.SubmitCullingDebugLines(Editor->GetWorld());

    // Shadow depth는 라이트 시점 → 뷰포트 무관. 프레임당 1회만 렌더링.
    ++Renderer.GetResources().ShadowResources.FrameGeneration;

    for (FLevelEditorViewportClient *ViewportClient : Editor->GetLevelViewportClients())
    {
        if (!Editor->ShouldRenderViewportClient(ViewportClient))
        {
            continue;
        }

        FEditorViewportRenderRequest Request;
        if (!ViewportClient->BuildRenderRequest(Request))
        {
            continue;
        }

        SCOPE_STAT_CAT("RenderViewport", "2_Render");
        RenderViewportRequest(Request, Renderer);
    }

    // Asset Editor Preview Viewport 렌더링.
    // LevelEditorViewportClient를 재사용하지 않고, 각 Asset Preview Client가 만든
    // FEditorViewportRenderRequest만 공통 렌더 경로로 넘긴다.
    TArray<FEditorViewportClient *> AssetViewportClients;
    Editor->CollectAssetViewportClients(AssetViewportClients);
    for (FEditorViewportClient *ViewportClient : AssetViewportClients)
    {
        if (!ViewportClient)
        {
            continue;
        }

        FEditorViewportRenderRequest Request;
        if (!ViewportClient->BuildRenderRequest(Request))
        {
            continue;
        }

        SCOPE_STAT_CAT("RenderAssetPreviewViewport", "2_Render");
        RenderViewportRequest(Request, Renderer);
    }

    // 스왑체인 백버퍼 복귀 → ImGui 합성 → Present
    Renderer.BeginFrame();
    {
        SCOPE_STAT_CAT("EditorUI", "5_UI");
        Editor->RenderUI(DeltaTime);
    }

#if STATS
    FGPUProfiler::Get().EndFrame();
#endif

    {
        SCOPE_STAT_CAT("Present", "2_Render");
        Renderer.EndFrame();
    }
}

void FEditorRenderPipeline::RenderViewportRequest(const FEditorViewportRenderRequest &Request, FRenderer &Renderer)
{
    if (!Request.Viewport || !Request.Scene)
    {
        return;
    }

    ID3D11DeviceContext *Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
    if (!Ctx)
    {
        return;
    }

    if (Request.LevelViewportClient && Request.World)
    {
        FLevelEditorViewportClient *VC = Request.LevelViewportClient;
        UWorld *World = Request.World;
        const FMinimalViewInfo *ViewInfo = &Request.ViewInfo;
        UCameraComponent *RuntimeCamera = nullptr;

        if (Editor && Editor->IsPIEPossessedMode())
        {
            if (UGameViewportClient *GameViewportClient = Editor->GetGameViewportClient())
            {
                if (UCameraComponent *GameCamera = GameViewportClient->GetDrivingCamera())
                {
                    if (IsAliveObject(GameCamera) && GameCamera->GetWorld() == World)
                    {
                        RuntimeCamera = GameCamera;
                        ViewInfo = &RuntimeCamera->GetCameraState();
                    }
                }
            }

            if (!RuntimeCamera)
            {
                if (UCameraComponent *ActiveCamera = World->GetActiveCamera())
                {
                    if (IsAliveObject(ActiveCamera) && ActiveCamera->GetWorld() == World)
                    {
                        RuntimeCamera = ActiveCamera;
                        ViewInfo = &RuntimeCamera->GetCameraState();
                    }
                }
            }
        }

        FGPUOcclusionCulling &GPUOcclusion = GetOcclusionForViewport(VC);
        GPUOcclusion.ReadbackResults(Ctx);

        PrepareViewport(Request.Viewport, Ctx);
        BuildFrame(VC, *ViewInfo, RuntimeCamera, Request.Viewport, World);

        FCollectOutput Output;
        CollectCommands(VC, World, Renderer, Output);

        {
            SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
            Renderer.Render(Frame, *Request.Scene);
        }

        if (Request.bEnableGPUOcclusion)
        {
            SCOPE_STAT_CAT("GPUOcclusion", "4_ExecutePass");
            GPUOcclusion.DispatchOcclusionTest(Ctx, Request.Viewport->GetDepthCopySRV(), Output.FrustumVisibleProxies,
                                               Frame.View, Frame.Proj, Request.Viewport->GetWidth(), Request.Viewport->GetHeight());
        }
        return;
    }

    PrepareViewport(Request.Viewport, Ctx);
    BuildFrameFromRequest(Request);

    FCollectOutput Output;
    CollectSceneCommands(Request, Renderer, Output);

    {
        SCOPE_STAT_CAT("Renderer.RenderViewportRequest", "4_ExecutePass");
        Renderer.Render(Frame, *Request.Scene);
    }
}

// ============================================================
// PrepareViewport — 지연 리사이즈 적용 + RT 클리어
// ============================================================
void FEditorRenderPipeline::PrepareViewport(FViewport *VP, ID3D11DeviceContext *Ctx)
{
    VP->ApplyPendingResize();
    const float ClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    VP->BeginRender(Ctx, ClearColor);
}

// ============================================================
// BuildFrame — FFrameContext 일괄 설정
// ============================================================
void FEditorRenderPipeline::BuildFrame(FLevelEditorViewportClient *VC, const FMinimalViewInfo& ViewInfo, UCameraComponent *LightReferenceCamera, FViewport *VP,
                                       UWorld *World)
{
    Frame.ClearViewportResources();
    const FMinimalViewInfo *ActivePOV = nullptr;
    if (World)
    {
        if (AGameModeBase *GameMode = World->GetAuthGameMode())
        {
            if (APlayerCameraManager *CameraManager = GameMode->GetPlayerCameraManager();
                CameraManager && CameraManager->HasValidCameraCachePOV())
            {
                ActivePOV = &CameraManager->GetCameraCachePOV();
            }
        }
    }

    if (ActivePOV)
    {
        Frame.SetCameraInfo(*ActivePOV);
    }
    else
    {
        Frame.SetCameraInfo(ViewInfo);
    }

    // Light View Override — 라이트 시점으로 View/Proj 교체
    if (VC->IsViewingFromLight())
    {
        ULightComponentBase *Light = VC->GetLightViewOverride();
        if (!Light || !Light->GetOwner())
        {
            VC->ClearLightViewOverride();
        }
        else
        {
            FLightViewProjResult LVP;
            if (Light->GetLightViewProj(LVP, LightReferenceCamera, VC->GetPointLightFaceIndex()))
            {
                Frame.View = LVP.View;
                Frame.Proj = LVP.Proj;
                Frame.bIsOrtho = LVP.bIsOrtho;
                Frame.CameraPosition = Light->GetWorldLocation();
                Frame.CameraForward = Light->GetForwardVector();
                Frame.FrustumVolume.UpdateFromMatrix(Frame.View * Frame.Proj);
            }
        }
    }

    Frame.bIsLightView = VC->IsViewingFromLight();
    Frame.SetRenderOptions(VC->GetRenderOptions());
    Frame.SetViewportInfo(VP);
    const FMinimalViewInfo &CameraState = ActivePOV ? *ActivePOV : ViewInfo;
    const float AR = CameraState.bConstrainAspectRatio
                         ? CameraState.LetterBoxingAspectW / CameraState.LetterBoxingAspectH
                         : CameraState.AspectRatio;
    Frame.ApplyConstrainedAR(AR);
    Frame.OcclusionCulling = &GetOcclusionForViewport(VC);
    Frame.LODContext = World->PrepareLODContext();

    // Cursor position relative to viewport (for 2.5D culling visualization)
    if (!VC->GetCursorViewportPosition(Frame.CursorViewportX, Frame.CursorViewportY))
    {
        Frame.CursorViewportX = UINT32_MAX;
        Frame.CursorViewportY = UINT32_MAX;
    }
}

void FEditorRenderPipeline::BuildFrameFromRequest(const FEditorViewportRenderRequest &Request)
{
    Frame.ClearViewportResources();
    Frame.SetCameraInfo(Request.ViewInfo);
    Frame.SetRenderOptions(Request.RenderOptions);
    Frame.RenderOptions.ShowFlags.bGrid = Request.bRenderGrid;
    Frame.SetViewportInfo(Request.Viewport);

    const FMinimalViewInfo &CameraState = Request.ViewInfo;
    const float AR = CameraState.bConstrainAspectRatio
                         ? CameraState.LetterBoxingAspectW / CameraState.LetterBoxingAspectH
                         : CameraState.AspectRatio;
    Frame.ApplyConstrainedAR(AR);

    Frame.bIsLightView = false;
    Frame.OcclusionCulling = nullptr;
    Frame.LODContext.CameraPos = Frame.CameraPosition;
    Frame.LODContext.bForceFullRefresh = true;
    Frame.LODContext.bValid = true;

    if (!Request.CursorProvider || !Request.CursorProvider->GetCursorViewportPosition(Frame.CursorViewportX, Frame.CursorViewportY))
    {
        Frame.CursorViewportX = UINT32_MAX;
        Frame.CursorViewportY = UINT32_MAX;
    }
}

// ============================================================
// CollectCommands — Scene 데이터 주입 + DrawCommand 생성
// ============================================================
//
// 3단계로 구성:
//   1. Proxy   — frustum cull → DrawCommand 즉시 생성 (메시/폰트/데칼)
//   2. Debug   — Scene에 디버그 데이터 주입 (Grid, DebugDraw, Octree, ShadowFrustum)
//   3. UI      — Scene에 오버레이 텍스트 주입
//
// 마지막에 BuildDynamicCommands가 Scene 주입 데이터를 DrawCommand로 변환.

void FEditorRenderPipeline::CollectCommands(FLevelEditorViewportClient *VC, UWorld *World, FRenderer &Renderer,
                                            FCollectOutput &Output)
{
    SCOPE_STAT_CAT("Collector", "3_Collect");

    FScene &Scene = World->GetScene();
    Scene.ClearFrameData();

    FDrawCommandBuilder &Builder = Renderer.GetBuilder();
    Builder.BeginCollect(Frame, Scene.GetProxyCount());

    const FShowFlags &Flags = Frame.RenderOptions.ShowFlags;

    // ── 1. 데이터 수집: frustum cull + visibility/occlusion 필터 ──
    {
        SCOPE_STAT_CAT("Collect", "3_Collect");
        Collector.Collect(World, Frame, Output);
    }

    // ── 2. Debug: Scene에 디버그 데이터 주입 ──
    {
        SCOPE_STAT_CAT("CollectDebug", "3_Collect");
        const bool bAllowDebugVisuals = World && World->GetWorldType() != EWorldType::PIE;
        if (bAllowDebugVisuals)
        {
            Collector.CollectGrid(Frame.RenderOptions.GridSpacing, Frame.RenderOptions.GridHalfLineCount, Scene);
            Scene.SetLightVisualizationSettings(
                Flags.bLightVisualization, Frame.RenderOptions.DirectionalLightVisualizationScale,
                Frame.RenderOptions.PointLightVisualizationScale, Frame.RenderOptions.SpotLightVisualizationScale);

            if (Flags.bShowShadowFrustum)
                Scene.SubmitShadowFrustumDebug(World, Frame);

            if (Flags.bSceneBVH)
                Collector.CollectSceneBVHDebug(World, Scene);

            if (Flags.bOctree)
                Collector.CollectOctreeDebug(World->GetOctree(), Scene);

            if (Flags.bWorldBound)
                Collector.CollectWorldBoundsDebug(World, Scene);

            Collector.CollectDebugDraw(Frame, Scene);
        }
        else
        {
            Scene.SetLightVisualizationSettings(false, 0.0f, 0.0f, 0.0f);
        }
    }

    // ── 3. 커맨드 일괄 생성 (프록시 + 동적) ──
    {
        SCOPE_STAT_CAT("BuildCommands", "3_Collect");
        Builder.BuildCommands(Frame, &Scene, Output);
    }
}

void FEditorRenderPipeline::CollectSceneCommands(const FEditorViewportRenderRequest &Request, FRenderer &Renderer,
                                                 FCollectOutput &Output)
{
    if (!Request.Scene)
    {
        return;
    }

    SCOPE_STAT_CAT("CollectorAssetPreview", "3_Collect");

    FScene &Scene = *Request.Scene;
    Scene.ClearFrameData();

    FDrawCommandBuilder &Builder = Renderer.GetBuilder();
    Builder.BeginCollect(Frame, Scene.GetProxyCount());

    Collector.CollectScene(Scene, Frame, Output);

    if (Request.bRenderGrid)
    {
        Collector.CollectGrid(Frame.RenderOptions.GridSpacing, Frame.RenderOptions.GridHalfLineCount, Scene);
    }

    Collector.CollectDebugDraw(Frame, Scene);

    Builder.BuildCommands(Frame, &Scene, Output);
}
