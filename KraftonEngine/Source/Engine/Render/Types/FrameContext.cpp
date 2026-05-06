#include "FrameContext.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"

// Function : Populate frame camera data from active camera component
// input : Camera
// Camera : camera component used when PlayerCameraManager cache is not available
void FFrameContext::SetCameraInfo(const UCameraComponent* Camera)
{
	if (!Camera)
	{
		return;
	}

	SetCameraInfo(Camera->GetCameraState());
}

// Function : Populate frame camera data from final PlayerCameraManager POV
// input : POV
// POV : final camera view after CalcCamera and camera modifiers
void FFrameContext::SetCameraInfo(const FMinimalViewInfo& POV)
{
	View = FMatrix::MakeViewMatrix(
		POV.Rotation.GetRightVector(),
		POV.Rotation.GetUpVector(),
		POV.Rotation.GetForwardVector(),
		POV.Location);

	if (POV.bIsOrthogonal)
	{
		const float HalfW = POV.OrthoWidth * 0.5f;
		const float HalfH = POV.AspectRatio > 0.0f ? HalfW / POV.AspectRatio : HalfW;
		Proj = FMatrix::OrthoLH(HalfW * 2.0f, HalfH * 2.0f, POV.NearZ, POV.FarZ);
	}
	else
	{
		Proj = FMatrix::PerspectiveFovLH(POV.FOV, POV.AspectRatio, POV.NearZ, POV.FarZ);
	}

	CameraPosition = POV.Location;
	CameraForward = POV.Rotation.GetForwardVector();
	CameraRight = POV.Rotation.GetRightVector();
	CameraUp = POV.Rotation.GetUpVector();
	bIsOrtho = POV.bIsOrthogonal;
	OrthoWidth = POV.OrthoWidth;
	NearClip = POV.NearZ;
	FarClip = POV.FarZ;
	PostProcessSettings = POV.PostPorcessSettings;

	FrustumVolume.UpdateFromMatrix(View * Proj);
}

void FFrameContext::SetViewportInfo(const FViewport* VP)
{
	ViewportWidth = static_cast<float>(VP->GetWidth());
	ViewportHeight = static_cast<float>(VP->GetHeight());
	ViewportRTV = VP->GetRTV();
	ViewportDSV = VP->GetDSV();
	SceneColorCopySRV = VP->GetSceneColorCopySRV();
	SceneColorCopyTexture = VP->GetSceneColorCopyTexture();
	ViewportRenderTexture = VP->GetRTTexture();
	DepthTexture = VP->GetDepthTexture();
	DepthCopyTexture = VP->GetDepthCopyTexture();
	DepthCopySRV = VP->GetDepthCopySRV();
	StencilCopySRV = VP->GetStencilCopySRV();
	NormalRTV = VP->GetNormalRTV();
	NormalSRV = VP->GetNormalSRV();
	CullingHeatmapRTV = VP->GetCullingHeatmapRTV();
	CullingHeatmapSRV = VP->GetCullingHeatmapSRV();
}
