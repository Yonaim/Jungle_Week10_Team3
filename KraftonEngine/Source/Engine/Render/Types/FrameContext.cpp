#include "FrameContext.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"

void FFrameContext::SetCameraInfo(const UCameraComponent* Camera)
{
	View            = Camera->GetViewMatrix();
	Proj            = Camera->GetProjectionMatrix();
	CameraPosition  = Camera->GetWorldLocation();
	CameraForward   = Camera->GetForwardVector();
	CameraRight     = Camera->GetRightVector();
	CameraUp        = Camera->GetUpVector();
	bIsOrtho        = Camera->IsOrthogonal();
	OrthoWidth      = Camera->GetOrthoWidth();
	NearClip        = Camera->GetCameraState().NearZ;
	FarClip         = Camera->GetCameraState().FarZ;

	// Per-viewport frustum — used by RenderCollector for inline frustum culling
	FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FFrameContext::SetCameraInfo(const FMinimalViewInfo& POV)
{
	const FRotator Rotation = POV.Rotation;

	View = FMatrix::MakeViewMatrix(
		Rotation.GetRightVector(),
		Rotation.GetUpVector(),
		Rotation.GetForwardVector(),
		POV.Location);

	if (!POV.bIsOrthogonal)
	{
		Proj = FMatrix::PerspectiveFovLH(POV.FOV, POV.AspectRatio, POV.NearZ, POV.FarZ);
	}
	else
	{
		const float HalfW = POV.OrthoWidth * 0.5f;
		const float HalfH = HalfW / POV.AspectRatio;
		Proj = FMatrix::OrthoLH(HalfW * 2.0f, HalfH * 2.0f, POV.NearZ, POV.FarZ);
	}

	CameraPosition = POV.Location;
	CameraForward = Rotation.GetForwardVector();
	CameraRight = Rotation.GetRightVector();
	CameraUp = Rotation.GetUpVector();
	bIsOrtho = POV.bIsOrthogonal;
	OrthoWidth = POV.OrthoWidth;
	NearClip = POV.NearZ;
	FarClip = POV.FarZ;

	FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FFrameContext::SetViewportInfo(const FViewport* VP)
{
	ViewportWidth    = static_cast<float>(VP->GetWidth());
	ViewportHeight   = static_cast<float>(VP->GetHeight());
	ViewportRTV             = VP->GetRTV();
	ViewportDSV             = VP->GetDSV();
	SceneColorCopySRV       = VP->GetSceneColorCopySRV();
	SceneColorCopyTexture   = VP->GetSceneColorCopyTexture();
	ViewportRenderTexture   = VP->GetRTTexture();
	DepthTexture            = VP->GetDepthTexture();
	DepthCopyTexture        = VP->GetDepthCopyTexture();
	DepthCopySRV            = VP->GetDepthCopySRV();
	StencilCopySRV          = VP->GetStencilCopySRV();
	NormalRTV               = VP->GetNormalRTV();
	NormalSRV               = VP->GetNormalSRV();
	CullingHeatmapRTV       = VP->GetCullingHeatmapRTV();
	CullingHeatmapSRV       = VP->GetCullingHeatmapSRV();
}
