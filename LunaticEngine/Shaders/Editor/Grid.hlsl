#include "Common/ConstantBuffers.hlsli"

cbuffer GridBuffer : register(b2)
{
	// CPU: EditorGridRenderer::FGridShaderConstants 와 레이아웃을 맞춘다.
	float GridSize;
	float Range;
	float LineThickness;
	float MajorLineInterval;
	float MajorLineThickness;
	float MinorIntensity;
	float MajorIntensity;
	float MaxDistance;
	float AxisThickness;
	float AxisLength;
	float AxisIntensity;
	float DrawAxisPass;
	float4 GridCenter;
	float4 GridAxisA;
	float4 GridAxisB;
	float4 AxisColorA;
	float4 AxisColorB;
	float4 AxisColorN;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float3 WorldPos : TEXCOORD0;
	float2 LocalPos : TEXCOORD1;
	float3 AxisColor : TEXCOORD2;
};

float ComputeGridLineAlpha(float2 coord, float2 derivative, float lineThickness)
{
	// frac 기반 반복 패턴 + fwidth AA: 거리/해상도 변화에도 라인 폭이 급격히 깨지지 않도록 한다.
	const float2 grid = abs(frac(coord - 0.5f) - 0.5f) / max(derivative, 0.0001f.xx);
	return saturate(lineThickness - min(grid.x, grid.y));
}

float ComputeAxisSuppressionAlpha(float2 coord, float2 derivative, float lineThickness)
{
	const float2 axisDistance = abs(coord) / max(derivative, 0.0001f.xx);
	return saturate(lineThickness - min(axisDistance.x, axisDistance.y));
}

float ComputeSingleAxisAlpha(float coord, float derivative, float lineThickness)
{
	return saturate(lineThickness - abs(coord) / max(derivative, 0.0001f));
}

float3 NormalizeSafe(float3 v, float3 fallbackDir)
{
	const float len2 = dot(v, v);
	if (len2 <= 1.0e-8f)
	{
		return fallbackDir;
	}
	return v * rsqrt(len2);
}

float3 ReconstructViewForwardWS()
{
	// InvViewProj로 NDC 중심 near/far를 역투영해 카메라 전방 벡터를 얻는다.
	float4 nearWS = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), InvViewProj);
	float4 farWS = mul(float4(0.0f, 0.0f, 1.0f, 1.0f), InvViewProj);
	nearWS.xyz /= max(nearWS.w, 1.0e-6f);
	farWS.xyz /= max(farWS.w, 1.0e-6f);
	return NormalizeSafe(farWS.xyz - nearWS.xyz, float3(0.0f, 0.0f, 1.0f));
}

VSOutput VS(uint VertexID : SV_VertexID)
{
	// VB 없이 SV_VertexID로 사각형(2 triangles)을 생성한다.
	static const float2 Positions[6] = {
		float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(-1.0f, 1.0f),
		float2(1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, 1.0f),
	};

	VSOutput output;
	if (DrawAxisPass < 0.5f)
	{
		// Grid pass: CPU가 준 중심/축/범위로 평면상의 월드 위치를 구성한다.
		const float2 localPos = Positions[VertexID];
		const float3 worldPos = GridCenter.xyz + GridAxisA.xyz * (localPos.x * Range) + GridAxisB.xyz * (localPos.y * Range);
		output.WorldPos = worldPos;
		output.LocalPos = localPos;
		output.AxisColor = 0.0f.xxx;
		output.Position = mul(mul(float4(worldPos, 1.0f), View), Projection);
		return output;
	}

	const uint axisVertexID = VertexID;
	// Axis pass: X/Y/Z 3개 축 스트립(축당 6 vertices)을 생성한다.
	const uint axisID = axisVertexID / 6;
	const uint localID = axisVertexID % 6;
	const float2 q = Positions[localID];
	const float axisT = q.x;
	const float stripHalfWidth = max(GridSize * 0.1f * max(AxisThickness, 1.0f), 0.01f);
	const float3 viewForwardWS = ReconstructViewForwardWS();

	float3 worldPos = 0.0f.xxx;
	float axisCoord = 0.0f;
	float3 axisColor = 0.0f.xxx;
	if (axisID == 0)
	{
		// 카메라와 축 방향에 수직인 방향으로 리본 폭을 잡아, 정투영 축 정렬에서도 두께가 사라지지 않게 한다.
		const float3 axisDir = float3(1.0f, 0.0f, 0.0f);
		float3 widthDir = NormalizeSafe(cross(viewForwardWS, axisDir), float3(0.0f, 1.0f, 0.0f));
		worldPos = axisDir * (axisT * AxisLength) + widthDir * (q.y * stripHalfWidth);
		axisCoord = q.y * stripHalfWidth;
		axisColor = AxisColorA.rgb;
	}
	else if (axisID == 1)
	{
		const float3 axisDir = float3(0.0f, 1.0f, 0.0f);
		float3 widthDir = NormalizeSafe(cross(viewForwardWS, axisDir), float3(1.0f, 0.0f, 0.0f));
		worldPos = axisDir * (axisT * AxisLength) + widthDir * (q.y * stripHalfWidth);
		axisCoord = q.y * stripHalfWidth;
		axisColor = AxisColorB.rgb;
	}
	else
	{
		const float3 axisDir = float3(0.0f, 0.0f, 1.0f);
		float3 widthDir = NormalizeSafe(cross(viewForwardWS, axisDir), float3(1.0f, 0.0f, 0.0f));
		worldPos = axisDir * (axisT * AxisLength) + widthDir * (q.y * stripHalfWidth);
		axisCoord = q.y * stripHalfWidth;
		axisColor = AxisColorN.rgb;
	}

	output.WorldPos = worldPos;
	output.LocalPos = float2(axisCoord, axisT);
	output.AxisColor = axisColor;
	output.Position = mul(mul(float4(worldPos, 1.0f), View), Projection);
	return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
	const float dist = distance(input.WorldPos, CameraWorldPos);
	// 원점 근처 과도한 겹침을 줄이기 위해 아주 가까운 픽셀은 마스킹.
	const float nearMask = step(0.5f, dist);
	// 거리 페이드: 카메라와 멀어질수록 부드럽게 투명해진다.
	const float fade = pow(saturate(1.0f - dist / MaxDistance), 2.0f);

	if (DrawAxisPass > 0.5f)
	{
		// 축 스트립 내부에서 중앙선만 남기도록 alpha를 생성한다.
		const float derivative = max(fwidth(input.LocalPos.x), 0.0001f);
		const float axisDrawWidth = derivative * max(AxisThickness * 3.0f, 1.0f);
		float axisAlpha = 1.0f - smoothstep(0.0f, axisDrawWidth, abs(input.LocalPos.x));
		axisAlpha *= 1.0f - smoothstep(0.85f, 1.0f, abs(input.LocalPos.y));
		float finalAlpha = axisAlpha * fade * AxisIntensity * nearMask;
		finalAlpha *= step(0.01f, finalAlpha);
		return float4(input.AxisColor, finalAlpha);
	}

	const float2 planeCoord = float2(dot(input.WorldPos, GridAxisA.xyz), dot(input.WorldPos, GridAxisB.xyz));
	const float2 minorCoord = planeCoord / GridSize;
	// 화면공간 미분값을 이용한 라인 두께 정규화(확대/축소 시 aliasing 완화).
	float2 minorDerivative = max(fwidth(input.LocalPos.xy) * (Range / GridSize), 0.0001f.xx);
	float minorAlpha = ComputeGridLineAlpha(minorCoord, minorDerivative, LineThickness);
	if (AxisThickness > 0.0f)
	{
		// 축선 주변에서는 일반 그리드 라인을 눌러 색이 과도하게 겹치지 않게 한다.
		minorAlpha *= (1.0f - ComputeAxisSuppressionAlpha(minorCoord, minorDerivative, max(LineThickness, AxisThickness)));
	}
	minorAlpha *= MinorIntensity * saturate(0.8f - max(minorDerivative.x, minorDerivative.y));

	const float majorGridSize = GridSize * max(MajorLineInterval, 1.0f);
	const float2 majorCoord = planeCoord / majorGridSize;
	float2 majorDerivative = max(fwidth(input.LocalPos.xy) * (Range / majorGridSize), 0.0001f.xx);
	float majorAlpha = ComputeGridLineAlpha(majorCoord, majorDerivative, MajorLineThickness);
	if (AxisThickness > 0.0f)
	{
		majorAlpha *= (1.0f - ComputeAxisSuppressionAlpha(majorCoord, majorDerivative, max(MajorLineThickness, AxisThickness)));
	}
	majorAlpha *= MajorIntensity * saturate(1.2f - max(majorDerivative.x, majorDerivative.y));

	const float gridAlpha = max(minorAlpha, majorAlpha) * fade;
	const float3 gridColor = lerp(float3(0.35f, 0.35f, 0.35f), float3(0.55f, 0.55f, 0.55f), saturate(majorAlpha));

	float axisAAlpha = 0.0f;
	float axisBAlpha = 0.0f;
	if (AxisThickness > 0.0f)
	{
		// Grid pass에서도 평면 축선(A/B)을 추가로 강조해 방향감을 준다.
		axisBAlpha = ComputeSingleAxisAlpha(minorCoord.x, minorDerivative.x, AxisThickness) * fade * AxisIntensity;
		axisAAlpha = ComputeSingleAxisAlpha(minorCoord.y, minorDerivative.y, AxisThickness) * fade * AxisIntensity;
	}

	float finalAlpha = max(gridAlpha, max(axisAAlpha, axisBAlpha)) * nearMask;
	finalAlpha *= step(0.01f, finalAlpha);

	const float totalWeight = gridAlpha + axisAAlpha + axisBAlpha;
	float3 color = gridColor;
	if (totalWeight > 0.0001f)
	{
		// 여러 요소(일반/주요/축)의 가중 평균으로 최종 색을 구성한다.
		color = (gridColor * gridAlpha + AxisColorA.rgb * axisAAlpha + AxisColorB.rgb * axisBAlpha) / totalWeight;
	}
	return float4(color, finalAlpha);
}
