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
    float3 orm = gTextureMapsLinear[matData.gORMIdx].Sample(gsamAnisotropicWrap, pin.TexC).rgb;
    if (matData.gORMIdx >= NUM_TEXTURE) orm = float3(1.0f, 1.0f, 0.0f);
    float ambientOcclusion = lerp(1.0, orm.r, saturate(matData.gOcclusionStrength));
    roughness = saturate(roughness * orm.g);
    float metal = saturate(matData.gMetallic * orm.b);

    float alpha = diffuseAlbedo.a;
    float3 baseColor = diffuseAlbedo.rgb;
    fresnelR0 = lerp(fresnelR0, baseColor, metal);

    PbrMaterial pbrMat = { baseColor, fresnelR0, max(roughness, 0.045f), metal };
    // Only the first light casts a shadow.
    float shadow = ShadowFactor(pin.ShadowPosH, bumpedNormalW);

    float3 directLight = ComputeLighting(gLights, pbrMat, pin.PosW,
        bumpedNormalW, toEyeW, shadow);

    // sky hemisphere
    float3 hemiTop = float3(0.55, 0.62, 0.80); // skyblue scaling
    float3 hemiBot = float3(0.08, 0.07, 0.06);
    float up = saturate(dot(bumpedNormalW, float3(0, 1, 0)));
    float3 hemi = lerp(hemiBot, hemiTop, up);

    float3 irradiance = gAmbientLight.rgb * hemi;
    float3 V = toEyeW;
    float NdotV = saturate(dot(bumpedNormalW, V));
    float3 F_ibl = FresnelSchlickRoughness(NdotV, pbrMat.F0, pbrMat.Roughness);
    float3 kS = F_ibl;
    float3 kD = (1.0 - kS) * (1.0 - pbrMat.Metallic);

    float3 diffuseIBL = irradiance * pbrMat.BaseColor / PI;
    float3 reflectDir = reflect(-V, bumpedNormalW);
    float skyFactor = saturate(reflectDir.y * 0.5f + 0.5f);
    float3 specEnv = gAmbientLight.rgb * lerp(hemiBot, hemiTop, skyFactor);
    float specStrength = lerp(1.0f, 0.1f, pbrMat.Roughness);
    float3 specularIBL = specEnv * (F_ibl * specStrength);

    float ambientShadowStrength = 0.7f;
    float visibility = lerp(1.0f, shadow, ambientShadowStrength);
    float3 ambient = ambientOcclusion * visibility * (kD * diffuseIBL + specularIBL);

    // add emissive result
    float3 emissive = matData.gEmissiveFactor;
    if (matData.gEmissiveIdx < NUM_TEXTURE)
        emissive *= gTextureMapsSRGB[matData.gEmissiveIdx].Sample(gsamLinearWrap, pin.TexC).rgb;
    float3 color = directLight + ambient + emissive * matData.gEmissiveStrength;
    color = color / (color + 1.0f);
    color = pow(saturate(color), 1.0f / 2.2f);

    return float4(color, alpha);
}