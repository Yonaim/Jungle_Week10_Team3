#include "DefaultRenderPipeline.h"

#include "Renderer.h"
#include "Engine/Runtime/Engine.h"
#include "Component/CameraComponent.h"
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

void FDefaultRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	Renderer.BeginFrame();

	Frame.ClearViewportResources();

	FDrawCommandBuilder& Builder = Renderer.GetBuilder();

	UWorld* World = Engine->GetWorld();
	UCameraComponent* Camera = World ? World->GetActiveCamera() : nullptr;

	// Game/Shipping에서 화면에 그리려면 FViewport(오프스크린 RT)가 필요.
	// UGameEngine::Init이 GameViewportClient에 FViewport를 세팅해뒀음.
	FViewport* VP = nullptr;
	if (UGameViewportClient* GVC = Engine->GetGameViewportClient())
	{
		VP = GVC->GetViewport();
	}

	FScene* Scene = nullptr;
	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();

	if (Camera && VP)
	{
		// 1) 오프스크린 RT 클리어 + 바인딩
		const float ClearColor[4] = { 0.10f, 0.10f, 0.12f, 1.0f };
		VP->BeginRender(Ctx, ClearColor);

		Frame.SetCameraInfo(Camera);
		Frame.SetViewportInfo(VP);

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

	// 오프스크린 RT 내용을 스왑체인 백버퍼로 복사 — ImGui 합성 없이도 화면에 출력되게 함.
	// BeginFrame이 백버퍼를 클리어한 뒤이므로 여기서 덮어써도 안전.
	if (VP && VP->GetRTTexture())
	{
		Renderer.GetFD3DDevice().CopyToBackbuffer(VP->GetRTTexture());
	}

	Renderer.EndFrame();
}
