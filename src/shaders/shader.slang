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

// struct uniform_data {
// 	mat4s proj;
// 	mat4s view;
// 	vec4s color;
// 	vec2s pos;
// 	vec2s test;
// 	float time;

struct ShaderData {
	float4x4 proj;
	float4x4 view;
	float4x4 model;
	float4 color;
	float2 pos;
	float time;
};

struct VSOutput {
	float4 Pos : SV_POSITION;
	float3 Norm;
	float2 UV;
	float4 color;
	float2 circle_pos;
};

struct test_uniforms {
	uint32_t tex_index;
};

struct push_const {
	ShaderData *shaderData;
	uint32_t tex_index;
};

//vk::binding(binding, set)
[[vk::binding(0, 0)]]
ConstantBuffer<test_uniforms> ubo;

[[vk::binding(0, 1)]]
Texture2D<float4> tex[];

[[vk::binding(1, 1)]]
SamplerState sampler;

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform push_const pc)
{
	ShaderData *shaderData = pc.shaderData;
	VSOutput output;
	// output.Pos = float4(input.Pos, 0.0, 1.0);
	output.Pos = mul(shaderData.proj, mul(shaderData.view, mul(shaderData.model, float4(input.Pos.xyz, 1.0))));
	output.Norm = input.norm;
	output.UV = input.UV;
	output.color = shaderData.color;
	output.circle_pos = shaderData.pos;
	return output;
}

float sd_circle(float2 v, float r)
{
	return length(v) - r;
}

[shader("fragment")]
float4 fragMain(VSOutput input, uniform push_const pc, float4 pos: SV_Position)
{
	float2 pos = pos.xy;
	// float2 circle_pos = float2(400, 300);
	// float map = sd_circle(input.circle_pos - pos, 100);
	float map = sd_circle(pos - input.circle_pos, 100);
	float3 dir = float3(0.3, 0.8, 0.0);
	float diff = dot(input.Norm, dir);

	diff = max(diff, 0.03);
	// float4 color = tex.Sample(input.UV);
	// float4 color = float4(0.4, 0.4, 0.4, 1.0);
	float4 color = tex[pc.tex_index].Sample(sampler, input.UV);
	if (color.a < 0.3f)
		discard;
	color *= diff;
	// color = shaderData.color;

	// if (map < 0.0001) {
	// 	// color = float4(ubo.dat.pos.x / 800.0, ubo.dat.pos.y / 600.0, 0.0, 1.0);
	// 	color = tex.Sample(sampler, input.UV);
	// }

	return float4(color);
}
