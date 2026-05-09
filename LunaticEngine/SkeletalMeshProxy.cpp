#include "SkeletalMeshProxy.h"
#include "Component/SkinnedMeshComponent.h"
#include "Mesh/StaticMeshCommon.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Materials/Material.h"
#include "Engine/Runtime/Engine.h"

FSkeletalMeshProxy::FSkeletalMeshProxy(USkinnedMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
}

USkinnedMeshComponent* FSkeletalMeshProxy::GetSkinnedMeshComponent() const
{
	return static_cast<USkinnedMeshComponent*>(GetOwner());
}

bool FSkeletalMeshProxy::HasValidGeometry() const
{
	const FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
	return LOD.DynamicVB.GetBuffer() != nullptr && LOD.VertexCount > 0;
}

void FSkeletalMeshProxy::FillDrawCommandBuffer(FDrawCommandBuffer& OutBuffer) const
{
	const FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
	OutBuffer.VB          = LOD.DynamicVB.GetBuffer();
	OutBuffer.VBStride    = LOD.DynamicVB.GetStride();
	OutBuffer.IB          = LOD.StaticIB.GetBuffer();
	OutBuffer.IndexCount  = LOD.IndexCount;
	OutBuffer.VertexCount = LOD.VertexCount;
}

void FSkeletalMeshProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FSkeletalMeshProxy::UpdateMesh()
{
	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	if (!Device) return;

	for (uint32 lod = 0; lod < LODCount; ++lod)
	{
		FSkeletalLODDrawData& LOD = LODData[lod];
		if (LOD.DynamicVB.GetBuffer() == nullptr)
			LOD.DynamicVB.Create(Device, 256, sizeof(FNormalVertex));
	}

	RebuildSectionDraws();
}

void FSkeletalMeshProxy::UpdateLOD(uint32 LODLevel)
{
	if (LODLevel >= LODCount) LODLevel = LODCount - 1;
	CurrentLOD = LODLevel;
}

void FSkeletalMeshProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	USkinnedMeshComponent* Comp = GetSkinnedMeshComponent();
	TArray<FNormalVertex>* Verts = Comp ? Comp->GetCPUSkinnedVertices() : nullptr;
	if (!Verts || Verts->empty()) return;

	ID3D11Device* Device = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDevice() : nullptr;
	ID3D11DeviceContext* Context = GEngine ? GEngine->GetRenderer().GetFD3DDevice().GetDeviceContext() : nullptr;
	if (!Device || !Context) return;

	FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
	uint32 VertCount = (uint32)Verts->size();

	if (LOD.DynamicVB.GetBuffer() == nullptr)
		LOD.DynamicVB.Create(Device, VertCount, sizeof(FNormalVertex));

	LOD.DynamicVB.EnsureCapacity(Device, VertCount);
	LOD.DynamicVB.Update(Context, Verts->data(), VertCount);
	LOD.VertexCount = VertCount;
}

void FSkeletalMeshProxy::RebuildSectionDraws()
{
	if (!DefaultMaterial)
	{
		DefaultMaterial = UMaterial::CreateTransient(
			ERenderPass::Opaque,
			EBlendState::Opaque,
			EDepthStencilState::Default,
			ERasterizerState::SolidBackCull,
			FShaderManager::Get().GetOrCreate(EShaderPath::UberLit));
	}

	SectionDraws.clear();
	if (!DefaultMaterial) return;

	const FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
	uint32 DrawCount = LOD.IndexCount > 0 ? LOD.IndexCount : LOD.VertexCount;
	SectionDraws.push_back({ DefaultMaterial, 0, DrawCount });
}
