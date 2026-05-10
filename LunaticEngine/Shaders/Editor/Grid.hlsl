#include "Common/ConstantBuffers.hlsli"

cbuffer GridBuffer : register(b2)
{
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
	float4 nearWS = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), InvViewProj);
	float4 farWS = mul(float4(0.0f, 0.0f, 1.0f, 1.0f), InvViewProj);
	nearWS.xyz /= max(nearWS.w, 1.0e-6f);
	farWS.xyz /= max(farWS.w, 1.0e-6f);
	return NormalizeSafe(farWS.xyz - nearWS.xyz, float3(0.0f, 0.0f, 1.0f));
}

VSOutput VS(uint VertexID : SV_VertexID)
{
	static const float2 Positions[6] = {
		float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(-1.0f, 1.0f),
		float2(1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, 1.0f),
	};

	VSOutput output;
	if (DrawAxisPass < 0.5f)
	{
		const float2 localPos = Positions[VertexID];
		const float3 worldPos = GridCenter.xyz + GridAxisA.xyz * (localPos.x * Range) + GridAxisB.xyz * (localPos.y * Range);
		output.WorldPos = worldPos;
		output.LocalPos = localPos;
		output.AxisColor = 0.0f.xxx;
		output.Position = mul(mul(float4(worldPos, 1.0f), View), Projection);
		return output;
	}

	const uint axisVertexID = VertexID;
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
	const float nearMask = step(0.5f, dist);
	const float fade = pow(saturate(1.0f - dist / MaxDistance), 2.0f);

	if (DrawAxisPass > 0.5f)
	{
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
	float2 minorDerivative = max(fwidth(input.LocalPos.xy) * (Range / GridSize), 0.0001f.xx);
	float minorAlpha = ComputeGridLineAlpha(minorCoord, minorDerivative, LineThickness);
	if (AxisThickness > 0.0f)
	{
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
		axisBAlpha = ComputeSingleAxisAlpha(minorCoord.x, minorDerivative.x, AxisThickness) * fade * AxisIntensity;
		axisAAlpha = ComputeSingleAxisAlpha(minorCoord.y, minorDerivative.y, AxisThickness) * fade * AxisIntensity;
	}

	float finalAlpha = max(gridAlpha, max(axisAAlpha, axisBAlpha)) * nearMask;
	finalAlpha *= step(0.01f, finalAlpha);

	const float totalWeight = gridAlpha + axisAAlpha + axisBAlpha;
	float3 color = gridColor;
	if (totalWeight > 0.0001f)
	{
		color = (gridColor * gridAlpha + AxisColorA.rgb * axisAAlpha + AxisColorB.rgb * axisBAlpha) / totalWeight;
	}
	return float4(color, finalAlpha);
}
