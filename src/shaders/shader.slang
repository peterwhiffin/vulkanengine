/* Copyright (c) 2025-2026, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

struct VSInput {
	float3 Pos;
	float3 norm;
	float2 UV;
};

struct ShaderData {
	float4x4 proj;
	float4x4 view;
};

struct entity {
	float4x4 model;
};

struct VSOutput {
	float4 Pos : SV_POSITION;
	float3 Norm;
	float2 UV;
};

struct push_const {
	ShaderData *shaderData;
	uint32_t tex_index;
};

//vk::binding(binding, set)
[[vk::binding(0, 0)]]
Texture2D<float4> tex[];

[[vk::binding(1, 0)]]
SamplerState sampler;

[[vk::binding(0, 1)]]
ConstantBuffer<entity> ebuf;

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform push_const pc)
{
	ShaderData *shaderData = pc.shaderData;
	VSOutput output;
	output.Pos = mul(shaderData.proj, mul(shaderData.view, mul(ebuf.model, float4(input.Pos.xyz, 1.0))));
	output.Norm = input.norm;
	output.UV = input.UV;
	return output;
}

float sd_circle(float2 v, float r)
{
	return length(v) - r;
}

[shader("fragment")]
float4 fragMain(VSOutput input, uniform push_const pc, float4 pos: SV_Position)
{
	float4 color = tex[pc.tex_index].Sample(sampler, input.UV);

	if (color.a < 0.3f)
		discard;

	float3 dir = float3(0.3, 0.8, 0.0);
	float diff = dot(input.Norm, dir);
	diff = max(diff, 0.03);
	color *= diff;

	return float4(color);
}
