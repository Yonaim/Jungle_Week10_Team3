#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"

class USkinnedMeshComponent;

class FSkeletalMeshProxy : public FPrimitiveSceneProxy
{
public:
	static constexpr uint32 MAX_LOD = 4;

	FSkeletalMeshProxy(USkinnedMeshComponent* InComponent);

	bool HasValidGeometry() const override;
	void FillDrawCommandBuffer(FDrawCommandBuffer& OutBuffer) const override;

	void UpdateMesh() override;
	void UpdateMaterial() override;
	void UpdateLOD(uint32 LODLevel) override;
	void UpdatePerViewport(const FFrameContext& Frame) override;

private:
	void RebuildSectionDraws();
	USkinnedMeshComponent* GetSkinnedMeshComponent() const;

	struct FSkeletalLODDrawData
	{
		FDynamicVertexBuffer DynamicVB;
		FIndexBuffer StaticIB;
		TArray<FMeshSectionDraw> SectionDraws;
		uint32 VertexCount = 0;
		uint32 IndexCount = 0;
	};

	FSkeletalLODDrawData LODData[MAX_LOD];
	uint32 LODCount = 1;
};
