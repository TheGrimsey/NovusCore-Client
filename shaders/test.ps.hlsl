
[[vk::binding(1, PER_PASS)]] SamplerState _sampler;

[[vk::binding(1, PER_DRAW)]] Texture2D<float4> _texture;

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VertexOutput input) : SV_Target
{
    return _texture.Sample(_sampler, input.uv);
}