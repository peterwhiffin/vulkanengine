/* Copyright (c) 2025-2026, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

struct VSInput {
	float2 Pos;
	float2 UV;
};

struct ShaderData {
	float4 color;
	float2 pos;
	float time;
};

struct VSOutput {
	float4 Pos : SV_POSITION;
	float2 UV;
	float4 color;
	float2 circle_pos;
};

struct test_uniforms {
	float4 color;
};

[[vk::binding(0, 0)]]
ConstantBuffer<test_uniforms> ubo;

uniform ShaderData *dats;

[shader("vertex")]
VSOutput vertMain(VSInput input, uniform ShaderData *shaderData)
{
	VSOutput output;
	output.Pos = float4(input.Pos, 0.0, 1.0);
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
float4 fragMain(VSOutput input, float4 pos: SV_Position)
{
	float2 pos = pos.xy;
	// float2 circle_pos = float2(400, 300);
	// float map = sd_circle(input.circle_pos - pos, 100);
	float map = sd_circle(pos - input.circle_pos, 100);

	float4 color = input.color;

	if (map < 0.0001) {
		// color = float4(1.0, 0.0, 0.0, 1.0);
		color = ubo.color;
	}

	return float4(color);
}
