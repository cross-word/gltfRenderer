//***************************************************************************************
// Default.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float2 TexC1 : TEXCOORD1;
    float4 TangentU : TANGENT; //xyz , w
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 ShadowPosH : POSITION0;
    float3 PosW : POSITION1;
    float3 NormalW : NORMAL;
    float4 Tangent : TANGENT;
    float2 TexC : TEXCOORD;
    float2 TexC1 : TEXCOORD1;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut)0.0f;

    // Fetch the material data.
    MaterialParam matData = gMaterialData[gMaterialIndex];

    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gObject[gObjectId].gWorlds);
    vout.PosW = posW.xyz;

    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = normalize(mul(vin.NormalL, (float3x3)gObject[gObjectId].gWorldInverseTranspose));

    vout.Tangent.w = vin.TangentU.w;
    vout.Tangent.xyz = normalize(mul(vin.TangentU.xyz, (float3x3)gObject[gObjectId].gWorldInverseTranspose));

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);

    // Output vertex attributes for interpolation across triangle.
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gObject[gObjectId].gTransform);
    vout.TexC = mul(texC, matData.MatTransform).xy;
    vout.TexC1 = vin.TexC1;
    // Generate projective tex-coords to project shadow map onto scene.
    vout.ShadowPosH = mul(posW, gLightViewProj);

    return vout;
}

float4 PS(VertexOut pin, bool isFrontFace : SV_IsFrontFace) : SV_Target
{
    // Fetch the material data.
    MaterialParam matData = gMaterialData[gMaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float  roughness = matData.Roughness;
    uint diffuseMapIndex = matData.DiffuseMapIndex;
    uint normalMapIndex = matData.NormalMapIndex;

    // Dynamically look up the texture in the array.
    // diffuse albedo in sRGB texture map.
    float2 uvBase = SelectUV(matData.DiffuseUV, pin.TexC, pin.TexC1);
    float4 base = (matData.DiffuseMapIndex < NUM_TEXTURE) ? gTextureMapsSRGB[matData.DiffuseMapIndex].Sample(gsamAnisotropicWrap, uvBase) : float4(matData.DiffuseAlbedo.rgb, 1.0);

    diffuseAlbedo.rgb *= base.rgb;
    diffuseAlbedo.a *= base.a;

    // bit1: AlphaMask
    if ((matData.Flags & 2u) != 0u) 
    {
        clip(diffuseAlbedo.a - matData.AlphaCutoff);
    }

    // Interpolating normal can unnormalize it, so renormalize it.
    float2 uvN = SelectUV(matData.NormalUV, pin.TexC, pin.TexC1);
    float3 nmap = (matData.NormalMapIndex < NUM_TEXTURE) ? gTextureMapsLinear[matData.NormalMapIndex].Sample(gsamAnisotropicWrap, uvN) : float3(0, 0, 1);
    // two-sided normal mapping
    //if ((matData.Flags & 1u) != 0u && !isFrontFace && (matData.NormalMapIndex < NUM_TEXTURE)) nmap.xy = -nmap.xy;
    float3 bumpedNormalW = NormalSampleToWorldSpace(nmap, normalize(pin.NormalW), pin.Tangent, matData.gNormalScale);

    // Uncomment to turn off normal mapping.
    // bumpedNormalW = pin.NormalW;

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    //occlusion,roughness,metalic in linear texture map
    float2 uvORM = SelectUV(matData.ORMUV, pin.TexC, pin.TexC1);
    float3 orm = (matData.gORMIdx < NUM_TEXTURE) ? gTextureMapsLinear[matData.gORMIdx].Sample(gsamAnisotropicWrap, uvORM).rgb : float3(1.0, 1.0, 0.0);
    // if there is occlusion texture, use that
    float ambientOcclusion = 1.0f;
    if (matData.gOcclusionIndex < NUM_TEXTURE)
    {
        float2 uvAO = SelectUV(matData.OcclusionUV, pin.TexC, pin.TexC1);
        ambientOcclusion = gTextureMapsLinear[matData.gOcclusionIndex].Sample(gsamAnisotropicWrap, uvAO).r;
        ambientOcclusion = lerp(1.0, ambientOcclusion, saturate(matData.gOcclusionStrength));
    }
    else
    {
        ambientOcclusion = 1.0f;
    }

    roughness = saturate(roughness * orm.g);
    roughness = max(roughness, 0.45);
    float metal = saturate(matData.gMetallic * orm.b);

    // Only the first light casts a shadow.
    float  s = ShadowFactor(pin.ShadowPosH, bumpedNormalW);
    float3 shadow = float3(s, s, s);

    const float shininess = (1.0 - roughness) * (1.0 - roughness);
    float3 directRGB = 0;
    [unroll]
        for (int i = 0; i < NUM_LIGHTS; ++i)
        {
            float3 d = BRDF_GGX(gLights[i], pin.PosW, bumpedNormalW, toEyeW, diffuseAlbedo.rgb, metal, roughness);
            directRGB += d * shadow; //only first shadow
        }
    // switch to phong shading
    //Material mat = { diffuseAlbedo, fresnelR0, shininess };
    //float4 directLight = ComputeLighting(gLights, mat, pin.PosW, bumpedNormalW, toEyeW, shadow);
    //directRGB = directLight.rgb;
    
    // hemisphere
    float3 hemiTop = float3(0.32, 0.33, 0.35);
    float3 hemiBot = float3(0.06, 0.06, 0.06);
    float up = saturate(dot(bumpedNormalW, float3(0, 1, 0)));
    float3 hemi = lerp(hemiBot, hemiTop, up);

    float3 N = SafeNormalize(bumpedNormalW);
    float3 V = SafeNormalize(toEyeW);
    float  NdotV = saturate(dot(N, V));

    // base reflectance
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), diffuseAlbedo.rgb, metal);
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD = (1.0 - F) * (1.0 - metal);
    kD = max(kD, 0.02);

    // diffuse IBL
    float3 irradiance = gIrradianceMap.Sample(gsamLinearClamp, N).rgb;
    float3 diffuseIBL = irradiance * diffuseAlbedo.rgb;

    // specular IBL
    float maxMip = gSpecularMipCountMinus1;
    float3 R = SafeNormalize(reflect(-V, N));
    bool noSpecMips = (maxMip <= 0.0);
    float lod = noSpecMips ? 0.0 : saturate(roughness) * maxMip;

#define FORCE_NO_SPEC_IBL 0
    float3 prefiltered = (noSpecMips || FORCE_NO_SPEC_IBL) ? 0.0 : gSpecularMap.SampleLevel(gsamLinearClamp, R, lod).rgb;
    float2 brdf = gBRDFLUT.Sample(gsamLinearClamp, float2(NdotV, roughness)).rg;

    // NaN guard
    prefiltered = any(isnan(prefiltered)) ? 0 : prefiltered;
    brdf = any(isnan(brdf)) ? 0 : brdf;

    float3 specularIBL = prefiltered * (F * brdf.x + brdf.y);
    float specOcclusion = saturate(ambientOcclusion + NdotV - 1.0f);
    specularIBL *= specOcclusion;
    float3 ibl = ambientOcclusion * (kD * diffuseIBL + specularIBL) * gIBLStrength;

    // ambient
    float3 ambientRGB = ambientOcclusion * (kD * gAmbientLight * hemi);
    const float ambientShadowStrength = 0.4;
    ambientRGB *= lerp(1.0, s, ambientShadowStrength);

    // emissive
    float3 emissive = matData.gEmissiveFactor;
    if (matData.gEmissiveIdx < NUM_TEXTURE)
    {
        float2 uvE = SelectUV(matData.EmissiveUV, pin.TexC, pin.TexC1);
        emissive *= gTextureMapsSRGB[matData.gEmissiveIdx].Sample(gsamLinearWrap, uvE).rgb;
    }

    float3 color = ambientRGB + directRGB + ibl + emissive * matData.gEmissiveStrength;
    color = ToneMapACESFast(color, gExposure);
    //color = pow(saturate(color), 1.0 / 2.2);
    return float4(color, diffuseAlbedo.a);
}