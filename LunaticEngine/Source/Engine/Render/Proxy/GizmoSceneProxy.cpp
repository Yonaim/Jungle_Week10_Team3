#include "Render/Proxy/GizmoSceneProxy.h"
#include "Component/GizmoComponent.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Types/FrameContext.h"
#include "Materials/Material.h"
#include "Object/ObjectFactory.h"

namespace
{
	FMatrix MakeRotationMatrixFromComponentAxes(const UGizmoComponent* Gizmo)
	{
		FVector Forward = Gizmo->GetForwardVector();
		FVector Right = Gizmo->GetRightVector();
		FVector Up = Gizmo->GetUpVector();

		if (Forward.Length() <= 1.0e-6f) Forward = FVector(1.0f, 0.0f, 0.0f);
		if (Right.Length() <= 1.0e-6f) Right = FVector(0.0f, 1.0f, 0.0f);
		if (Up.Length() <= 1.0e-6f) Up = FVector(0.0f, 0.0f, 1.0f);

		Forward.Normalize();
		Right.Normalize();
		Up.Normalize();

		FMatrix RotationOnly = FMatrix::Identity;
		RotationOnly.M[0][0] = Forward.X; RotationOnly.M[0][1] = Forward.Y; RotationOnly.M[0][2] = Forward.Z;
		RotationOnly.M[1][0] = Right.X;   RotationOnly.M[1][1] = Right.Y;   RotationOnly.M[1][2] = Right.Z;
		RotationOnly.M[2][0] = Up.X;      RotationOnly.M[2][1] = Up.Y;      RotationOnly.M[2][2] = Up.Z;
		return RotationOnly;
	}
}

// ============================================================
// FGizmoSceneProxy
// ============================================================
FGizmoSceneProxy::FGizmoSceneProxy(UGizmoComponent* InComponent, bool bInner)
	: FPrimitiveSceneProxy(InComponent)
	, bIsInner(bInner)
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate
	            | EPrimitiveProxyFlags::NeverCull;
	ProxyFlags &= ~(EPrimitiveProxyFlags::SupportsOutline
	              | EPrimitiveProxyFlags::ShowAABB);

	GizmoMaterial = UMaterial::CreateTransient(
		bInner ? ERenderPass::GizmoInner : ERenderPass::GizmoOuter,
		bInner ? EBlendState::AlphaBlend : EBlendState::Opaque,
		bInner ? EDepthStencilState::GizmoInside : EDepthStencilState::GizmoOutside,
		ERasterizerState::SolidBackCull,
		FShaderManager::Get().GetOrCreate(EShaderPath::Gizmo));
}

FGizmoSceneProxy::~FGizmoSceneProxy()
{
	GizmoCB.Release();
	if (GizmoMaterial)
	{
		UObjectManager::Get().DestroyObject(GizmoMaterial);
		GizmoMaterial = nullptr;
	}
}

UGizmoComponent* FGizmoSceneProxy::GetGizmoComponent() const
{
	return static_cast<UGizmoComponent*>(GetOwner());
}

// ============================================================
// UpdateMesh — 현재 Gizmo 모드에 맞는 메시 버퍼 + 셰이더 캐싱
// ============================================================
void FGizmoSceneProxy::UpdateMesh()
{
	UGizmoComponent* Gizmo = GetGizmoComponent();
	MeshBuffer = Gizmo->GetMeshBuffer();
	RebuildGizmoSectionDraws();
}

// ============================================================
// UpdatePerViewport — 매 프레임 뷰포트별 스케일 + GizmoCB 갱신
// ============================================================
void FGizmoSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	UGizmoComponent* Gizmo = GetGizmoComponent();

	if (!Frame.RenderOptions.ShowFlags.bGizmo || !Gizmo->IsVisible() || !Gizmo->HasTarget() || Gizmo->GetMode() == EGizmoMode::Select)
	{
		bVisible = false;
		return;
	}
	bVisible = true;

	// 모드 변경 시 메시가 바뀌므로 매 프레임 갱신
	MeshBuffer = Gizmo->GetMeshBuffer();
	RebuildGizmoSectionDraws();

	// Per-viewport 스케일 계산
	const FVector CameraPos = Frame.View.GetInverseFast().GetLocation();
	float PerViewScale = Gizmo->ComputeScreenSpaceScale(
		CameraPos, Frame.bIsOrtho, Frame.OrthoWidth);

	const FMatrix RotationOnly = MakeRotationMatrixFromComponentAxes(Gizmo);
	FMatrix WorldMatrix = FMatrix::MakeScaleMatrix(FVector(PerViewScale, PerViewScale, PerViewScale))
		* RotationOnly
		* FMatrix::MakeTranslationMatrix(Gizmo->GetWorldLocation());

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(WorldMatrix);
	MarkPerObjectCBDirty();

	// GizmoMaterial에 Gizmo CB 바인딩
	auto& G = GizmoMaterial->BindPerShaderCB<FGizmoConstants>(
		&GizmoCB,
		ECBSlot::PerShader0);
	G.ColorTint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	G.bIsInnerGizmo = bIsInner ? 1 : 0;
	G.bClicking = Gizmo->IsHolding() ? 1 : 0;
	G.SelectedAxis = Gizmo->GetSelectedAxis() >= 0
		? static_cast<uint32>(Gizmo->GetSelectedAxis())
		: 0xFFFFFFFFu;
	G.HoveredAxisOpacity = 0.7f;
	G.AxisMask = UGizmoComponent::ComputeAxisMask(Frame.RenderOptions.ViewportType, Gizmo->GetMode());
}

void FGizmoSceneProxy::RebuildGizmoSectionDraws()
{
	SectionDraws.clear();
	if (MeshBuffer && GizmoMaterial)
	{
		uint32 IdxCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
		SectionDraws.push_back({ GizmoMaterial, 0, IdxCount });
	}
}


