//***************************************************************************************
// Common.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 24
#endif

#ifndef NUM_TEXTURE
#define NUM_TEXTURE 72
#endif

#ifndef NUM_LIGHTS
#define NUM_LIGHTS 25
#endif
// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

struct MaterialParam
{
    float4   DiffuseAlbedo;       // baseColorFactor
    float3   FresnelR0;  float Roughness;
    float4x4 MatTransform;

    float    gMetallic;
    float    gNormalScale;
    float    gOcclusionStrength;
    float    gEmissiveStrength;

    float3   gEmissiveFactor;

    uint     DiffuseMapIndex;
    uint     NormalMapIndex;
    uint     gORMIdx;          // G=Roughness, B=Metallic
    uint     gOcclusionIndex;  // R=AO
    uint     gEmissiveIdx;
};

struct ObjectParam
{
    float4x4 gWorlds;
    float4x4 gWorldInverseTranspose;
    float4x4 gTransform;
};

Texture2D    gTextureMapsSRGB[NUM_TEXTURE] : register(t0, space0);
Texture2D    gTextureMapsLinear[NUM_TEXTURE] : register(t0, space2);
StructuredBuffer<MaterialParam> gMaterialData : register(t0, space1);
StructuredBuffer<ObjectParam>   gObject    : register(t1, space1);
Texture2D gShadowMap : register(t2, space1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;

    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    float4x4 gLightViewProj;
    float2 gShadowTexelSize; float2 _padShadow;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[NUM_LIGHTS];
};

cbuffer cbMaterialIndex : register(b2)
{
    uint gMaterialIndex;
    uint gTexIndex;
    uint gObjectId;
};

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float4 tangent, float normalScale)
{
    // [0,1] => [-1,1]
    float3 normalT = 2.0f * normalMapSample - 1.0f; //on tangent space

    // Build orthonormal basis.
    float3 N = normalize(unitNormalW);
    float3 T = tangent.xyz;
    T = normalize(T);
    T = normalize(T - N * dot(T, N));
    float3 B = tangent.w * normalize(cross(N, T));

    float3x3 TBN = float3x3(T, B, N);

    // Transform from tangent space to world space.
    float3 bumpedNormalW = normalize(mul(normalT, TBN));

    return bumpedNormalW;
}

float ShadowFactor(float4 shadowPosH, float3 N)
{
    float3 proj = shadowPosH.xyz / shadowPosH.w;
    float2 uv = proj.xy * float2(0.5f, -0.5f) + 0.5f;

    // out of frustum
    if (proj.z < 0 || proj.z > 1 || any(uv < 0) || any(uv > 1))
        return 1.0;

    float3 Ldir = normalize(-gLights[0].Direction);
    float NdotL = saturate(dot(N, Ldir));

    uint w, h, mips; gShadowMap.GetDimensions(0, w, h, mips);
    float2 texel = 1.0 / float2(w, h);
    float receiverPlaneBias = max(texel.x, texel.y);
    float baseBias = receiverPlaneBias * (1.5f + proj.z);
    float slopeBias = receiverPlaneBias * (2.0f * (1.0f - NdotL));
    float bias = baseBias + slopeBias;

    // poisson offset
    const float2 poisson[16] =
    {
        float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
        float2(-0.09418410, -0.92938870), float2(0.34495938,  0.29387760),
        float2(-0.91588581,  0.45771432), float2(-0.81544232, -0.87912464),
        float2(-0.38277543,  0.27676845), float2(0.97484398,  0.75648379),
        float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420),
        float2(-0.26496911, -0.41893023), float2(0.79197514,  0.19090188),
        float2(-0.24188840,  0.99706507), float2(-0.81409955,  0.91437590),
        float2(0.19984126,  0.78641367), float2(0.14383161, -0.14100790)
    };
    float radius = 2.0 + 2.0 * proj.z;

    float lit = 0;
    [unroll] for (int i = 0; i < 16; ++i)
    {
        lit += gShadowMap.SampleCmpLevelZero(gsamShadow, uv + poisson[i] * radius * texel, proj.z - bias).r;
    }
    return lit * (1.0 / 16.0); // [0,1]
}