//***************************************************************************************
// SilhouetteBlur.hlsl - Gaussian blur for static pixels (no velocity)
//
// Two-pass separable Gaussian blur applied only to pixels with zero/near-zero velocity
// Pass 1: Horizontal blur
// Pass 2: Vertical blur
//***************************************************************************************

cbuffer cbBlur : register(b0)
{
    float2 gScreenSize;
    float2 gBlurDirection;  // (1,0) for horizontal, (0,1) for vertical
    float gVelocityThreshold;  // Threshold below which pixel is considered static
    float gBlurRadius;         // Blur strength
    float2 gPadding;
};

Texture2D gInputTexture   : register(t0);
Texture2D gMotionVectors  : register(t1);

SamplerState gsamPointClamp  : register(s1);
SamplerState gsamLinearClamp : register(s3);

struct VertexOut
{
    float4 PosH  : SV_POSITION;
    float2 TexC  : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    vout.TexC = float2((vid << 1) & 2, vid & 2);
    vout.PosH = float4(vout.TexC * float2(2, -2) + float2(-1, 1), 0, 1);
    return vout;
}

// Gaussian weights for 9-tap kernel (sigma ~= 2.0)
static const float gWeights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

float4 PS(VertexOut pin) : SV_Target
{
    float2 uv = pin.TexC;
    float2 texelSize = 1.0f / gScreenSize;
    
    // Sample velocity at current pixel
    float2 velocity = gMotionVectors.Sample(gsamPointClamp, uv).rg;
    float velocityMagnitude = length(velocity * gScreenSize);  // In pixels
    
    // If pixel has significant velocity, return original color (no blur)
    if (velocityMagnitude > gVelocityThreshold)
    {
        return gInputTexture.Sample(gsamPointClamp, uv);
    }
    
    // Calculate blur mask based on velocity (smooth transition)
    float blurMask = 1.0f - saturate(velocityMagnitude / gVelocityThreshold) + 0.1f;
    
    // Apply Gaussian blur
    float2 blurOffset = gBlurDirection * texelSize * gBlurRadius;
    
    float3 result = gInputTexture.Sample(gsamLinearClamp, uv).rgb * gWeights[0];
    
    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        float2 offset = blurOffset * float(i);
        result += gInputTexture.Sample(gsamLinearClamp, uv + offset).rgb * gWeights[i];
        result += gInputTexture.Sample(gsamLinearClamp, uv - offset).rgb * gWeights[i];
    }
    
    // Blend between original and blurred based on velocity mask
    float3 original = gInputTexture.Sample(gsamPointClamp, uv).rgb;
    float3 finalColor = lerp(original, result, blurMask);
    
    return float4(finalColor, 1.0f);
}
