#include "Mesh/FbxImporter.h"
#include "Mesh/FbxCommon.h"

#include "Core/Log.h"
#include "Mesh/SkeletalMeshCommon.h"
#include "Mesh/StaticMeshCommon.h"

#if defined(_WIN64)
namespace
{
	void ConvertMeshNode(const FString& FbxFilePath, FbxNode* Node, FFbxInfo& Context, FSkeletalMesh& OutMesh)
	{
		FbxMesh* Mesh = Node ? Node->GetMesh() : nullptr;
		if (!Mesh)
		{
			return;
		}

		const char* UVSetName = nullptr;
		FbxStringList UVSetNames;
		Mesh->GetUVSetNames(UVSetNames);
		if (UVSetNames.GetCount() > 0)
		{
			UVSetName = UVSetNames[0];
		}

		const FbxAMatrix GlobalTransform = Node->EvaluateGlobalTransform();
		std::unordered_map<FFbxVertexKey, uint32, FFbxVertexKeyHash> VertexMap;
		const int32 PolygonCount = Mesh->GetPolygonCount();

		for (int32 PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
		{
			if (Mesh->GetPolygonSize(PolygonIndex) != 3)
			{
				continue;
			}

			int32 LocalMaterialIndex = FFbxCommon::GetPolygonMaterialIndex(Mesh, PolygonIndex);
			if (LocalMaterialIndex < 0)
			{
				LocalMaterialIndex = 0;
			}

			const int32 MaterialIndex = FFbxCommon::FindOrAddMaterial(FbxFilePath, Node, LocalMaterialIndex, Context);
			if (static_cast<size_t>(MaterialIndex) >= Context.FacesPerMaterial.size())
			{
				Context.FacesPerMaterial.resize(static_cast<size_t>(MaterialIndex) + 1);
			}

			uint32 TriangleIndices[3] = {};
			for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
			{
				const int32 ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);
				const FFbxVertexKey Key{ ControlPointIndex, PolygonIndex, CornerIndex, MaterialIndex };

				auto It = VertexMap.find(Key);
				if (It != VertexMap.end())
				{
					TriangleIndices[CornerIndex] = It->second;
					continue;
				}

				FNormalVertex Vertex;
				Vertex.pos = FFbxCommon::RemapVector(GlobalTransform.MultT(Mesh->GetControlPointAt(ControlPointIndex)));

				FbxVector4 FbxNormal(0.0, 0.0, 1.0, 0.0);
				Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, FbxNormal);
				Vertex.normal = FFbxCommon::RemapVector(GlobalTransform.MultR(FbxNormal)).Normalized();

				FbxVector2 FbxUV(0.0, 0.0);
				bool bUnmapped = false;
				Vertex.tex = UVSetName && Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetName, FbxUV, bUnmapped) && !bUnmapped
					? FFbxCommon::RemapUV(FbxUV)
					: FVector2(0.0f, 0.0f);
				Vertex.color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

				const uint32 NewVertexIndex = static_cast<uint32>(OutMesh.Vertices.size());
				OutMesh.Vertices.push_back(Vertex);
				VertexMap.emplace(Key, NewVertexIndex);
				TriangleIndices[CornerIndex] = NewVertexIndex;
			}

			const uint32 FaceStart = static_cast<uint32>(OutMesh.Indices.size());
			OutMesh.Indices.push_back(TriangleIndices[0]);
			OutMesh.Indices.push_back(TriangleIndices[2]);
			OutMesh.Indices.push_back(TriangleIndices[1]);
			Context.FacesPerMaterial[MaterialIndex].push_back(FaceStart);
		}
	}

	void TraverseNodes(const FString& FbxFilePath, FbxNode* Node, FFbxInfo& Context, FSkeletalMesh& OutMesh)
	{
		if (!Node)
		{
			return;
		}

		ConvertMeshNode(FbxFilePath, Node, Context, OutMesh);

		const int32 ChildCount = Node->GetChildCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			TraverseNodes(FbxFilePath, Node->GetChild(ChildIndex), Context, OutMesh);
		}
	}

	void BuildSections(FSkeletalMesh& OutMesh, const TArray<FStaticMaterial>& OutMaterials, const TArray<TArray<uint32>>& FacesPerMaterial)
	{
		TArray<uint32> OldIndices = std::move(OutMesh.Indices);
		OutMesh.Indices.clear();
		OutMesh.Sections.clear();

		for (size_t MaterialIndex = 0; MaterialIndex < FacesPerMaterial.size() && MaterialIndex < OutMaterials.size(); ++MaterialIndex)
		{
			const TArray<uint32>& FaceStarts = FacesPerMaterial[MaterialIndex];
			if (FaceStarts.empty())
			{
				continue;
			}

			FStaticMeshSection Section;
			Section.MaterialIndex = static_cast<int32>(MaterialIndex);
			Section.MaterialSlotName = OutMaterials[MaterialIndex].MaterialSlotName;
			Section.FirstIndex = static_cast<uint32>(OutMesh.Indices.size());
			Section.NumTriangles = static_cast<uint32>(FaceStarts.size());

			for (uint32 FaceStart : FaceStarts)
			{
				OutMesh.Indices.push_back(OldIndices[FaceStart + 0]);
				OutMesh.Indices.push_back(OldIndices[FaceStart + 1]);
				OutMesh.Indices.push_back(OldIndices[FaceStart + 2]);
			}

			//OutMesh.Sections.push_back(Section);
		}
	}

	void AccumulateTangents(FSkeletalMesh& Mesh)
	{
		TArray<FVector> TangentSums(Mesh.Vertices.size(), FVector(0.0f, 0.0f, 0.0f));
		TArray<FVector> BitangentSums(Mesh.Vertices.size(), FVector(0.0f, 0.0f, 0.0f));

		for (size_t i = 0; i + 2 < Mesh.Indices.size(); i += 3)
		{
			const uint32 I0 = Mesh.Indices[i + 0];
			const uint32 I1 = Mesh.Indices[i + 1];
			const uint32 I2 = Mesh.Indices[i + 2];

			const FNormalVertex& V0 = Mesh.Vertices[I0];
			const FNormalVertex& V1 = Mesh.Vertices[I1];
			const FNormalVertex& V2 = Mesh.Vertices[I2];

			const FVector Edge1 = V1.pos - V0.pos;
			const FVector Edge2 = V2.pos - V0.pos;
			const FVector2 DeltaUV1 = V1.tex - V0.tex;
			const FVector2 DeltaUV2 = V2.tex - V0.tex;

			const float Det = DeltaUV1.X * DeltaUV2.Y - DeltaUV1.Y * DeltaUV2.X;
			if (std::abs(Det) < 1e-8f)
			{
				continue;
			}

			const float InvDet = 1.0f / Det;
			const FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDet;
			const FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * InvDet;

			TangentSums[I0] += Tangent;
			TangentSums[I1] += Tangent;
			TangentSums[I2] += Tangent;
			BitangentSums[I0] += Bitangent;
			BitangentSums[I1] += Bitangent;
			BitangentSums[I2] += Bitangent;
		}

		for (size_t i = 0; i < Mesh.Vertices.size(); ++i)
		{
			FNormalVertex& Vertex = Mesh.Vertices[i];
			FVector Normal = Vertex.normal.Normalized();
			FVector Tangent = TangentSums[i];
			Tangent = (Tangent - Normal * Normal.Dot(Tangent)).Normalized();
			if (Tangent.IsNearlyZero())
			{
				Tangent = FVector(1.0f, 0.0f, 0.0f);
			}

			const float Handedness = Normal.Cross(Tangent).Dot(BitangentSums[i]) < 0.0f ? -1.0f : 1.0f;
			Vertex.tangent = FVector4(Tangent, Handedness);
		}
	}

	bool ConvertSkeletalMesh(const FString& FbxFilePath, FFbxInfo& Context, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
	{
		OutMesh = FSkeletalMesh();
		OutMaterials.clear();

		TraverseNodes(FbxFilePath, Context.Scene ? Context.Scene->GetRootNode() : nullptr, Context, OutMesh);

		if (Context.Materials.empty() && !OutMesh.Indices.empty())
		{
			FFbxMaterialInfo DefaultMaterial;
			Context.Materials.push_back(DefaultMaterial);
			Context.FacesPerMaterial.resize(1);
			for (size_t FaceStart = 0; FaceStart + 2 < OutMesh.Indices.size(); FaceStart += 3)
			{
				Context.FacesPerMaterial[0].push_back(static_cast<uint32>(FaceStart));
			}
		}

		for (const FFbxMaterialInfo& MaterialInfo : Context.Materials)
		{
			FStaticMaterial StaticMaterial;
			StaticMaterial.MaterialInterface = FMaterialManager::Get().GetOrCreateMaterial(FFbxCommon::ConvertMaterialInfoToMat(MaterialInfo));
			StaticMaterial.MaterialSlotName = MaterialInfo.MaterialSlotName;
			OutMaterials.push_back(StaticMaterial);
		}

		BuildSections(OutMesh, OutMaterials, Context.FacesPerMaterial);
		OutMesh.PathFileName = FbxFilePath;
		//OutMesh.CacheBounds();
		AccumulateTangents(OutMesh);

		return !OutMesh.Vertices.empty() && !OutMesh.Indices.empty();
	}
}
bool FFbxSkeletalMeshImporter::Import(const FString& FbxFilePath, FSkeletalMesh& OutMesh, TArray<FStaticMaterial>& OutMaterials)
{
	FFbxInfo Context;
	if (!FFbxCommon::ParseFbx(FbxFilePath, Context))
	{
		return false;
	}

	const bool bImported = ConvertSkeletalMesh(FbxFilePath, Context, OutMesh, OutMaterials);
	FFbxCommon::Destroy(Context);
	return bImported;
}
#endif
