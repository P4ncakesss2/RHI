struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D<float4> sourceImage : register(t0);
SamplerState sourceSampler : register(s0);

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID) {
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0),
    };

    float2 position = positions[vertexID];
    VSOutput output;
    output.position = float4(position, 0.0, 1.0);
    output.uv = float2(position.x * 0.5 + 0.5, 0.5 - position.y * 0.5);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target {
    return sourceImage.Sample(sourceSampler, input.uv);
}
