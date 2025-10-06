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

    // Generate projective tex-coords to project shadow map onto scene.
    vout.ShadowPosH = mul(posW, gLightViewProj);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
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
    diffuseAlbedo *= gTextureMapsSRGB[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    if (diffuseMapIndex >= NUM_TEXTURE) diffuseAlbedo = matData.DiffuseAlbedo;
#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    clip(diffuseAlbedo.a - 0.1f);
#endif

    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);
    float4 normalMapSample = gTextureMapsLinear[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.Tangent, matData.gNormalScale);
    if (normalMapIndex >= NUM_TEXTURE) bumpedNormalW = pin.NormalW;
    // Uncomment to turn off normal mapping.
    // bumpedNormalW = pin.NormalW;

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(gEyePosW - pin.PosW);

    //occlusion,roughness,metalic in linear texture map
    float3 orm = (matData.gORMIdx < NUM_TEXTURE) ? gTextureMapsLinear[matData.gORMIdx].Sample(gsamAnisotropicWrap, pin.TexC).rgb : float3(1.0f, 1.0f, 0.0f);

    // if there is occlusion texture, use that
    float aoTex = (matData.gOcclusionIndex < NUM_TEXTURE) ? gTextureMapsLinear[matData.gOcclusionIndex].Sample(gsamAnisotropicWrap, pin.TexC).r : orm.r;
    float ambientOcclusion = lerp(1.0f, aoTex, saturate(matData.gOcclusionStrength));

    roughness = saturate(roughness * orm.g);
    roughness = max(roughness, 0.06f);
    float metal = saturate(matData.gMetallic * orm.b);

    // Only the first light casts a shadow.
    float  s = ShadowFactor(pin.ShadowPosH, bumpedNormalW);
    float3 shadowFactor = float3(s, s, s);

    const float shininess = (1.0 - roughness) * (1.0 - roughness);
    fresnelR0 = lerp(fresnelR0, diffuseAlbedo.rgb, metal);
    diffuseAlbedo.rgb *= (1.0 - metal);

    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, bumpedNormalW, toEyeW, shadowFactor);

    // sky hemisphere
    float3 hemiTop = float3(0.55, 0.62, 0.80); // skyblue scaling
    float3 hemiBot = float3(0.08, 0.07, 0.06);
    float up = saturate(dot(bumpedNormalW, float3(0, 1, 0)));
    float3 hemi = lerp(hemiBot, hemiTop, up);

    // IBL (diffuse + specular)
    float3 N = SafeNormalize(bumpedNormalW);
    float3 V = SafeNormalize(toEyeW);
    float  NdotV = saturate(dot(N, V));

    // base reflectance
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), diffuseAlbedo.rgb, metal);
    float3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD = (1.0 - F) * (1.0 - metal);
    kD = max(kD, 0.02);

    // final ambient
    float3 ambientRGB = ambientOcclusion * (kD * gAmbientLight * hemi);
    const float ambientShadowStrength = 0.7;
    ambientRGB *= lerp(1.0, s, ambientShadowStrength);
    float4 ambient = float4(ambientRGB, 0.0);

    // diffuse IBL
    float3 irradiance = gIrradianceMap.Sample(gsamLinearClamp, N).rgb;
    float3 diffuseIBL = irradiance * diffuseAlbedo.rgb;

    // specular IBL
    float  maxMip = max(gSpecularMipCountMinus1, 0.0);
    float3 R = SafeNormalize(reflect(-V, N));
    float  mip = clamp(roughness * maxMip, 0.0, maxMip);
    float3 prefiltered = gSpecularMap.SampleLevel(gsamLinearClamp, R, mip).rgb;
    float2 brdf = gBRDFLUT.Sample(gsamLinearClamp, float2(NdotV, roughness)).rg;

    // NaN guard
    prefiltered = any(isnan(prefiltered)) ? 0 : prefiltered;
    brdf = any(isnan(brdf)) ? 0 : brdf;

    float3 specularIBL = prefiltered * (F * brdf.x + brdf.y);
    float3 ibl = ambientOcclusion * (kD * diffuseIBL + specularIBL) * gIBLStrength;

    // emissive
    float3 emissive = matData.gEmissiveFactor;
    if (matData.gEmissiveIdx < NUM_TEXTURE) emissive *= gTextureMapsSRGB[matData.gEmissiveIdx].Sample(gsamLinearWrap, pin.TexC).rgb;

    float3 color = (ambientRGB + directLight.rgb + ibl + emissive * matData.gEmissiveStrength);
    color = ToneMapACESFast(color, gExposure);    // tone mapping

    return float4(color, diffuseAlbedo.a);
}