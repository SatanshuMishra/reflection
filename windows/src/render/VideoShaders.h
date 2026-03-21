#pragma once

namespace reflection::shaders {

/// Fullscreen triangle vertex shader.
/// Generates a triangle that covers the entire screen from vertex ID alone.
/// No vertex buffer required — just call Draw(3, 0).
///
/// Vertex 0: (-1, -1) → UV (0, 1)   (bottom-left)
/// Vertex 1: ( 3, -1) → UV (2, 1)   (far right)
/// Vertex 2: (-1,  3) → UV (0, -1)  (far top)
///
/// The GPU clips to the viewport, producing a fullscreen quad.
constexpr const char* kVideoVertexShader = R"hlsl(
struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertex_id : SV_VertexID) {
    VSOutput output;

    // Generate fullscreen triangle from vertex ID
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;

    return output;
}
)hlsl";

/// NV12 → RGB pixel shader.
/// Samples the Y plane (R8_UNORM) and UV plane (R8G8_UNORM) from an NV12
/// texture and converts to sRGB using the BT.601 color matrix.
///
/// NV12 layout:
///   - Y plane: luminance (0-1 from R8_UNORM)
///   - UV plane: chrominance (0-1 from R8G8_UNORM, bias at 0.5)
///
/// BT.601 full-range conversion:
///   R = Y + 1.402 * (V - 0.5)
///   G = Y - 0.344136 * (U - 0.5) - 0.714136 * (V - 0.5)
///   B = Y + 1.772 * (U - 0.5)
constexpr const char* kVideoPixelShader = R"hlsl(
Texture2D texY  : register(t0);
Texture2D texUV : register(t1);
SamplerState samp : register(s0);

struct PSInput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target {
    float y = texY.Sample(samp, input.uv).r;
    float2 uv = texUV.Sample(samp, input.uv).rg;

    // Offset chroma from [0,1] to [-0.5, 0.5]
    float u = uv.r - 0.5;
    float v = uv.g - 0.5;

    // BT.601 YUV to RGB conversion
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;

    return float4(saturate(float3(r, g, b)), 1.0);
}
)hlsl";

} // namespace reflection::shaders
