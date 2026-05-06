#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "Component/CameraComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/World.h"
#include "Viewport/Viewport.h"
#include "Viewport/GameViewportClient.h"

FDefaultRenderPipeline::FDefaultRenderPipeline(UEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
}

FDefaultRenderPipeline::~FDefaultRenderPipeline()
{
}

// Function : Execute default render pipeline for current frame
// input : DeltaTime, Renderer
// DeltaTime : frame delta time
// Renderer : renderer that consumes final frame context and scene draw commands
void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Renderer.BeginFrame();

	Frame.ClearViewportResources();

	FDrawCommandBuilder& Builder = Renderer.GetBuilder();

	UWorld* World = Engine->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;

	FViewport* VP = nullptr;
	if (UGameViewportClient* GVC = Engine->GetGameViewportClient())
	{
		VP = GVC->GetViewport();
	}

	if (VP)
	{
		const uint32 BackbufferWidth = Renderer.GetFD3DDevice().GetBackbufferWidth();
		const uint32 BackbufferHeight = Renderer.GetFD3DDevice().GetBackbufferHeight();
		if (BackbufferWidth > 0 && BackbufferHeight > 0
			&& (VP->GetWidth() != BackbufferWidth || VP->GetHeight() != BackbufferHeight))
		{
			VP->RequestResize(BackbufferWidth, BackbufferHeight);
		}

		if (VP->ApplyPendingResize())
		{
			Renderer.ResetRenderStateCache();
		}
	}

	FScene* Scene = nullptr;
	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();

	if (Camera && VP)
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));

		const float ClearColor[4] = { 0.10f, 0.10f, 0.12f, 1.0f };
		VP->BeginRender(Ctx, ClearColor);

		Frame.SetViewportInfo(VP);

		AGameModeBase* GameMode = World->GetAuthGameMode();
		APlayerCameraManager* CameraManager = GameMode ? GameMode->GetPlayerCameraManager() : nullptr;
		if (CameraManager && CameraManager->HasValidCameraCachePOV())
		{
			Frame.SetCameraInfo(CameraManager->GetCameraCachePOV());
		}
		else
		{
			Frame.SetCameraInfo(Camera);
		}

		FViewportRenderOptions Opts;
		Opts.ViewMode = EViewMode::Lit_Phong;
		Frame.SetRenderOptions(Opts);

		Scene = &World->GetScene();
		Scene->ClearFrameData();

		Builder.BeginCollect(Frame, Scene->GetProxyCount());
		FCollectOutput Output;
		Collector.Collect(World, Frame, Output);
		Collector.CollectDebugDraw(Frame, *Scene);
		Builder.BuildCommands(Frame, Scene, Output);
	}
	else
	{
		Builder.BeginCollect(Frame);
		FCollectOutput EmptyOutput;
		Builder.BuildCommands(Frame, nullptr, EmptyOutput);
	}

	if (Scene)
	{
		Renderer.Render(Frame, *Scene);
	}

	if (VP && VP->GetRTTexture())
	{
		Renderer.GetFD3DDevice().CopyToBackbuffer(VP->GetRTTexture());
	}

	if (Engine)
	{
		Engine->RenderImGuiOverlay(Renderer);
	}

	Renderer.EndFrame();
}
