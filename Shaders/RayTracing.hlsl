//***************************************************************************************
// RayTracing.hlsl - Ray tracing shader approximating Default.hlsl lighting.
//***************************************************************************************
// Defaults for number of lights.
#include "Common.hlsl"

RaytracingAccelerationStructure gScene : register(t0, space4);
RWTexture2D<float4> gOutput : register(u0);
Texture2D<float4> gAccumOutput : register(t1, space4);

struct GeometryMetadata
{
    uint IndexOffset;   // First index in the global index buffer for this geometry.
    uint VertexOffset;  // First vertex in the global vertex buffer for this geometry.
    uint MaterialIndex; // Index into gMaterialData.
    uint _padding;
};

StructuredBuffer<GeometryMetadata> gGeometryTable : register(t4, space1);
StructuredBuffer<uint>   gIndices   : register(t0, space3);
StructuredBuffer<float3> gPositions : register(t1, space3);
StructuredBuffer<float3> gNormals   : register(t2, space3);
StructuredBuffer<float2> gTexCoords : register(t3, space3);
StructuredBuffer<float4> gTangents  : register(t4, space3);

struct RadiancePayload
{
    float3 radiance;
    uint rayDepth;
};

struct ShadowPayload
{
    float visibility; // 0 hit, 1 miss
};

struct Attributes
{
    float2 barycentrics;
};

static const uint MAX_RAY_DEPTH = 1;
#ifndef NUM_RAY_SAMPLES
#define NUM_RAY_SAMPLES 4
#endif
#ifndef TEMPORAL_ALPHA
#define TEMPORAL_ALPHA 0.1f
#endif

uint Hash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float RandomFloat(uint seed)
{
    return (float)(Hash(seed)) * 2.3283064365386963e-10f;
}

float2 GenerateSubpixelJitter(uint2 pixel, uint sampleIdx, uint frameSeed)
{
    uint seed = pixel.x * 374761393u + pixel.y * 668265263u + frameSeed * 362437u + sampleIdx * 1231u;
    float jx = RandomFloat(seed);
    float jy = RandomFloat(seed + 1u);
    return float2(jx, jy);
}
static uint GeoIndex() { return InstanceID(); }

float3 EvaluateEnvironment(float3 dir)
{
    // sky hemisphere
    float up = saturate(dir.y * 0.5f + 0.5f);
    float3 hemiTop = float3(0.55f, 0.62f, 0.80f);
    float3 hemiBot = float3(0.08f, 0.07f, 0.06f);
    return lerp(hemiBot, hemiTop, up) * gAmbientLight.rgb;
}

float SampleAlpha(uint primitiveIndex, float2 barycentrics)
{
    GeometryMetadata geo = gGeometryTable[GeoIndex()];

    uint triIndex = geo.IndexOffset + primitiveIndex * 3;
    uint i0 = gIndices[triIndex + 0] + geo.VertexOffset;
    uint i1 = gIndices[triIndex + 1] + geo.VertexOffset;
    uint i2 = gIndices[triIndex + 2] + geo.VertexOffset;

    float2 uv0 = gTexCoords[i0];
    float2 uv1 = gTexCoords[i1];
    float2 uv2 = gTexCoords[i2];

    float3 bary = float3(1.0f - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
    float2 tex = uv0 * bary.x + uv1 * bary.y + uv2 * bary.z;

    uint materialIndex = geo.MaterialIndex;
    uint materialCount, materialStride;
    gMaterialData.GetDimensions(materialCount, materialStride);
    if (materialIndex >= materialCount)
        return 1.0f;

    MaterialParam matData = gMaterialData[materialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    if (matData.DiffuseMapIndex < NUM_TEXTURE)
        diffuseAlbedo *= gTextureMapsSRGB[matData.DiffuseMapIndex].SampleLevel(gsamAnisotropicWrap, tex, 0);

    return diffuseAlbedo.a;
}

// role of PS in rasterize shader
float3 ShadeSurface(uint primitiveIndex, float2 barycentrics, inout RadiancePayload payload, out float outAlpha)
{
    GeometryMetadata geo = gGeometryTable[GeoIndex()];

    // gather vertex info
    uint triIndex = geo.IndexOffset + primitiveIndex * 3;
    uint index0 = gIndices[triIndex + 0] + geo.VertexOffset;
    uint index1 = gIndices[triIndex + 1] + geo.VertexOffset;
    uint index2 = gIndices[triIndex + 2] + geo.VertexOffset;

    float3 pos0 = gPositions[index0];
    float3 pos1 = gPositions[index1];
    float3 pos2 = gPositions[index2];

    float2 tex0 = gTexCoords[index0];
    float2 tex1 = gTexCoords[index1];
    float2 tex2 = gTexCoords[index2];

    float3 normal0 = gNormals[index0];
    float3 normal1 = gNormals[index1];
    float3 normal2 = gNormals[index2];

    float4 tangent0 = gTangents[index0];
    float4 tangent1 = gTangents[index1];
    float4 tangent2 = gTangents[index2];

    float3 bary = float3(1.0f - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);

    //linear interpolatino
    float3 posL = pos0 * bary.x + pos1 * bary.y + pos2 * bary.z;
    float3 normalL = normalize(normal0 * bary.x + normal1 * bary.y + normal2 * bary.z);
    float2 tex = tex0 * bary.x + tex1 * bary.y + tex2 * bary.z;
    float4 tangent = normalize(tangent0 * bary.x + tangent1 * bary.y + tangent2 * bary.z);

    float3x4 objToWorld = ObjectToWorld3x4();
    float3x4 worldToObject = WorldToObject3x4();

    float3 posW = mul(objToWorld, float4(posL, 1.0f));
    float3x3 worldToObject3x3 = (float3x3)worldToObject;
    float3 normalW = normalize(mul(normalL, (float3x3)transpose(worldToObject3x3)));
    tangent.xyz = normalize(mul(tangent.xyz, (float3x3)transpose(worldToObject3x3)));

    uint materialIndex = geo.MaterialIndex;
    uint materialCount, materialStride;
    gMaterialData.GetDimensions(materialCount, materialStride);
    if (materialIndex >= materialCount)
    {
        outAlpha = 1.0f;
        return float3(1.0f, 0.0f, 1.0f);
    }

    MaterialParam matData = gMaterialData[materialIndex];

    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;

    diffuseAlbedo *= gTextureMapsSRGB[matData.DiffuseMapIndex].SampleLevel(gsamAnisotropicWrap, tex, 0);
    if (matData.DiffuseMapIndex >= NUM_TEXTURE) diffuseAlbedo = matData.DiffuseAlbedo;

    if (diffuseAlbedo.a < 0.1f)
    {
        outAlpha = 0.0f;
        return float3(0.0f, 0.0f, 0.0f);
    }

    // Interpolating normal can unnormalize it, so renormalize it.
    float3 bumpedNormal = normalW;
    float3 normalMapSample = gTextureMapsLinear[matData.NormalMapIndex].SampleLevel(gsamAnisotropicWrap, tex, 0).rgb;
    bumpedNormal = NormalSampleToWorldSpace(normalMapSample, normalW, tangent, matData.gNormalScale);
    if (matData.NormalMapIndex >= NUM_TEXTURE) bumpedNormal = normalW;

    // Vector from point being lit to eye. 
    float3 toEye = normalize(gEyePosW - posW);

    //occlusion,roughness,metalic in linear texture map
    float3 orm = gTextureMapsLinear[matData.gORMIdx].SampleLevel(gsamAnisotropicWrap, tex, 0).rgb;
    if (matData.gORMIdx >= NUM_TEXTURE) orm = float3(1.0f, 1.0f, 0.0f);

    float ambientOcclusion = lerp(1.0f, orm.r, saturate(matData.gOcclusionStrength));
    roughness = clamp(saturate(roughness * orm.g), 0.45f, 0.90f);
    float metal = saturate(matData.gMetallic * orm.b);

    const float shininess = (1.0f - roughness) * (1.0f - roughness);
    fresnelR0 = lerp(fresnelR0, diffuseAlbedo.rgb, metal);
    diffuseAlbedo.rgb *= (1.0f - metal);

    float shadow = 1.0f;
    if (payload.rayDepth < MAX_RAY_DEPTH)
    {
        RayDesc shadowRay;
        shadowRay.Origin = posW + bumpedNormal * 0.01f;
        shadowRay.Direction = normalize(-gLights[0].Direction);
        shadowRay.TMin = 0.01f;
        shadowRay.TMax = 1e38f;

        ShadowPayload shadowPayload;
        shadowPayload.visibility = 1.0f;

        TraceRay(gScene,
            RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
            0xFF,
            1,
            2,
            1,
            shadowRay,
            shadowPayload);

        shadow = shadowPayload.visibility;
    }

    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float4 directLight = ComputeLighting(gLights, mat, posW, bumpedNormal, toEye, float3(shadow, shadow, shadow));

    // ambient F0 approximation
    float3 hemi = EvaluateEnvironment(bumpedNormal);
    float3 kS = lerp(float3(0.04f, 0.04f, 0.04f), matData.DiffuseAlbedo.rgb, metal);
    float3 kD = (1.0f - kS) * (1.0f - metal);

    // final ambient
    float3 ambientRGB = ambientOcclusion * kD * hemi;
    const float ambientShadowStrength = 0.7;
    ambientRGB *= lerp(1.0, shadow, ambientShadowStrength);
    float4 ambient = float4(ambientRGB, 0.0);

    // phong shading result) ambient + diffuse + specular
    float4 litColor = ambient + directLight;

    // add emissive result
    float3 emissive = matData.gEmissiveFactor;
    if (matData.gEmissiveIdx < NUM_TEXTURE)
        emissive *= gTextureMapsSRGB[matData.gEmissiveIdx].SampleLevel(gsamLinearWrap, tex, 0).rgb;
    litColor.rgb += emissive * matData.gEmissiveStrength;

    //litColor.rgb = pow(saturate(litColor.rgb), 1.0 / 2.2);// gammma cor

    outAlpha = diffuseAlbedo.a;
    return litColor.rgb;
}

// role of VS in rasterize shader
[shader("raygeneration")]
void RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;

    float3 accumulatedRadiance = 0.0f;
    uint frameSeed = asuint(gTotalTime * 1000.0f); // jitter var

    [loop]
        for (uint sampleIdx = 0; sampleIdx < NUM_RAY_SAMPLES; ++sampleIdx)
        {
            float2 jitter = GenerateSubpixelJitter(pixel, sampleIdx, frameSeed);
            float2 sampleUv = (float2(pixel) + jitter) / float2(dim);
            float2 ndc = float2(sampleUv.x * 2.0f - 1.0f, 1.0f - sampleUv.y * 2.0f);

            float4 target = mul(float4(ndc, 0.0f, 1.0f), gInvProj);
            target.xyz /= target.w;
            float3 world = mul(float4(target.xyz, 1.0f), gInvView).xyz;

            RayDesc ray;
            ray.Origin = gEyePosW;
            ray.Direction = normalize(world - gEyePosW);
            ray.TMin = 0.001f;
            ray.TMax = 1e38f;

            RadiancePayload payload;
            payload.radiance = 0.0f;
            payload.rayDepth = 0;

            TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, payload);


            accumulatedRadiance += payload.radiance; //accumulate results
        }
    gOutput[pixel] = float4(accumulatedRadiance / NUM_RAY_SAMPLES, 1.0f); //record avg
}

[shader("miss")]
void Miss(inout RadiancePayload payload)
{
    payload.radiance = EvaluateEnvironment(-WorldRayDirection()); //hemisphere
}

[shader("closesthit")]
void ClosestHit(inout RadiancePayload payload, Attributes attr)
{
    if (payload.rayDepth >= MAX_RAY_DEPTH)
    {
        payload.radiance = 0.0f;
        return;
    }

    float alpha;
    float3 color = ShadeSurface(PrimitiveIndex(), attr.barycentrics, payload, alpha); //cal pixel shader result of a pixel
    payload.radiance = color;
    payload.rayDepth++;
}

[shader("anyhit")]
void ShadowAnyHit(inout ShadowPayload payload, Attributes attr)
{
    float alpha = SampleAlpha(PrimitiveIndex(), attr.barycentrics);
    if (alpha < 0.1f)
    {
        IgnoreHit();
        return;
    }

    payload.visibility = 0.0f;
    AcceptHitAndEndSearch();
}

[shader("miss")]
void ShadowMiss(inout ShadowPayload payload)
{
    payload.visibility = 1.0f;
}