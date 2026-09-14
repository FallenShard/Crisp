
struct InterpolatedVertex {
    float2 texCoord;
};

struct Fragment {
    float4 color;
};

[[vk::binding(0, 0)]]
Sampler2D tex;

float toSrgb(float value) {
    if (value < 0.0031308f) {
        return 12.92f * value;
    }
    return 1.055f * pow(value, 0.41666f) - 0.055f;
}

[shader("fragment")]
Fragment main(InterpolatedVertex in : InterpolatedVertex) : SV_Target {
    const float4 texel = tex.Sample(in.texCoord);

    Fragment output;
    output.color = float4(toSrgb(texel.r), toSrgb(texel.g), toSrgb(texel.b), texel.a);
    return output;
}