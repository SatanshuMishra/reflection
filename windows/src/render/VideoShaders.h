// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

namespace reflection::shaders {

/// Fullscreen triangle vertex shader.
/// Generates a triangle covering the entire screen from vertex ID alone.
/// No vertex buffer required — just call Draw(3, 0).
constexpr const char* kVideoVertexShader = R"hlsl(
struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertex_id : SV_VertexID) {
    VSOutput output;
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}
)hlsl";

/// Passthrough pixel shader for BGRA textures.
/// Simply samples the texture and outputs the color directly.
/// Used when the MFT decoder outputs RGB32/ARGB32 (DXGI_FORMAT_B8G8R8A8_UNORM).
constexpr const char* kPassthroughPixelShader = R"hlsl(
Texture2D tex : register(t0);
SamplerState samp : register(s0);

struct PSInput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    return tex.Sample(samp, input.uv);
}
)hlsl";

} // namespace reflection::shaders
