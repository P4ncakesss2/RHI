struct VSInput {
    float2 position : POSITION;
    float3 color : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 color    : COLOR;
};

cbuffer Transform : register(b31) {
    float2 offset;
    float rotation;
    float _pad;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input) {
    float c = cos(rotation);
    float s = sin(rotation);
    float2 rotated = float2(
        input.position.x * c - input.position.y * s,
        input.position.x * s + input.position.y * c
    );

    VSOutput output;
    output.position = float4(rotated + offset, 0.0, 1.0);
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target {
    return float4(input.color, 1.0);
}
