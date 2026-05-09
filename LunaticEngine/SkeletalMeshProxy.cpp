#include "SkeletalMeshProxy.h"
#include "Component/SkinnedMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/StaticMeshCommon.h"
#include "Render/Types/FrameContext.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
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

	USkinnedMeshComponent* Comp = GetSkinnedMeshComponent();
	USkeletalMesh* SkelMesh = Comp ? Comp->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = SkelMesh ? SkelMesh->GetSkeletalMeshAsset() : nullptr;

	for (uint32 lod = 0; lod < LODCount; ++lod)
	{
		FSkeletalLODDrawData& LOD = LODData[lod];

		if (LOD.DynamicVB.GetBuffer() == nullptr)
		{
			uint32 InitialCount = Asset ? (uint32)Asset->Vertices.size() : 256;
			LOD.DynamicVB.Create(Device, InitialCount, sizeof(FNormalVertex));
		}

		if (Asset && !Asset->Indices.empty() && LOD.StaticIB.GetBuffer() == nullptr)
		{
			uint32 IdxCount = (uint32)Asset->Indices.size();
			LOD.StaticIB.Create(Device, Asset->Indices.data(), IdxCount, IdxCount * sizeof(uint32));
			LOD.IndexCount  = IdxCount;
			LOD.VertexCount = (uint32)Asset->Vertices.size();
		}
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
	USkinnedMeshComponent* Comp = GetSkinnedMeshComponent();
	USkeletalMesh* SkelMesh = Comp ? Comp->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = SkelMesh ? SkelMesh->GetSkeletalMeshAsset() : nullptr;

	SectionDraws.clear();

	if (!Asset || Asset->Sections.empty())
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
		if (!DefaultMaterial) return;

		const FSkeletalLODDrawData& LOD = LODData[CurrentLOD];
		uint32 DrawCount = LOD.IndexCount > 0 ? LOD.IndexCount : LOD.VertexCount;
		SectionDraws.push_back({ DefaultMaterial, 0, DrawCount });
		return;
	}

	const TArray<FStaticMaterial>& Slots = SkelMesh->GetStaticMaterials();

	for (const FSkeletalMeshSection& Section : Asset->Sections)
	{
		UMaterial* Mat = nullptr;

		if (Section.MaterialIndex >= 0 && Section.MaterialIndex < (int32)Slots.size())
			Mat = Slots[Section.MaterialIndex].MaterialInterface;

		if (!Mat)
			Mat = FMaterialManager::Get().GetOrCreateMaterial("None");

		SectionDraws.push_back({ Mat, Section.IndexStart, Section.IndexCount });
	}
}
