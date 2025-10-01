//***************************************************************************************
// RayTracing.hlsl - Ray tracing shader approximating Default.hlsl lighting.
//***************************************************************************************

#include "Common.hlsl"

RaytracingAccelerationStructure gScene : register(t0, space4);
RWTexture2D<float4> gOutput : register(u0);

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
    uint depth;
};

struct ShadowPayload
{
    float visibility;
};

struct Attributes
{
    float2 barycentrics;
};

static const uint MAX_RAY_DEPTH = 1;
static uint GeoIndex() { return InstanceID(); }

float3 EvaluateEnvironment(float3 dir)
{
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

float ComputeLOD(float t /*RayTCurrent()*/, float texelsPerMeter, uint mips)
{
    float fp = max(1e-3, t * texelsPerMeter);      // 히트 거리 * 텍셀 밀도
    float lod = log2(fp);                          // 대략적 LOD
    return clamp(lod, 0.0f, (float)(mips - 1));
}

float TexelsPerMeter(float2 uv0, float2 uv1, float2 uv2,
    float3 p0, float3 p1, float3 p2, uint texW, uint texH)
{
    float2 duv1 = uv1 - uv0, duv2 = uv2 - uv0;
    float3 dp1 = p1 - p0, dp2 = p2 - p0;
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) < 1e-6) return max(texW, texH);   // 퇴화 삼각형 보호

    // 월드 미터당 UV 변화량의 역수를 이용해 텍셀/미터 근사
    float3 dpdu = (dp1 * duv2.y - dp2 * duv1.y) / det;
    float3 dpdv = (-dp1 * duv2.x + dp2 * duv1.x) / det;
    float metersPerU = length(dpdu);
    float metersPerV = length(dpdv);
    float texelPerU = texW / max(1e-4, metersPerU);
    float texelPerV = texH / max(1e-4, metersPerV);
    return max(texelPerU, texelPerV);
}

float3 ShadeSurface(uint primitiveIndex, float2 barycentrics, inout RadiancePayload payload, out float outAlpha)
{
    GeometryMetadata geo = gGeometryTable[GeoIndex()];

    uint triIndex = geo.IndexOffset + primitiveIndex * 3;
    uint i0 = gIndices[triIndex + 0] + geo.VertexOffset;
    uint i1 = gIndices[triIndex + 1] + geo.VertexOffset;
    uint i2 = gIndices[triIndex + 2] + geo.VertexOffset;

    float3 p0 = gPositions[i0];
    float3 p1 = gPositions[i1];
    float3 p2 = gPositions[i2];

    float2 uv0 = gTexCoords[i0];
    float2 uv1 = gTexCoords[i1];
    float2 uv2 = gTexCoords[i2];

    float3 n0 = gNormals[i0];
    float3 n1 = gNormals[i1];
    float3 n2 = gNormals[i2];

    float4 t0 = gTangents[i0];
    float4 t1 = gTangents[i1];
    float4 t2 = gTangents[i2];

    float3 bary = float3(1.0f - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);

    float3 posL = p0 * bary.x + p1 * bary.y + p2 * bary.z;
    float3 normalL = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
    float2 tex = uv0 * bary.x + uv1 * bary.y + uv2 * bary.z;
    float4 tangent = normalize(t0 * bary.x + t1 * bary.y + t2 * bary.z);

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

    if (matData.DiffuseMapIndex < NUM_TEXTURE)
    {
        //diffuseAlbedo *= gTextureMapsSRGB[matData.DiffuseMapIndex].SampleLevel(gsamAnisotropicWrap, tex, 0);
        uint tw, th, mips;
        gTextureMapsSRGB[matData.DiffuseMapIndex].GetDimensions(0, tw, th, mips);
        float t = RayTCurrent();
        float tpm = TexelsPerMeter(uv0, uv1, uv2, p0, p1, p2, tw, th);
        float lod = ComputeLOD(t, tpm, mips);
        lod = clamp(lod - 1.25, 0.0f, min((float)mips - 1.0f, 4.0f));
        diffuseAlbedo *= gTextureMapsSRGB[matData.DiffuseMapIndex].SampleLevel(gsamAnisotropicWrap, tex, lod);
    }
    else
        diffuseAlbedo = matData.DiffuseAlbedo;

    if (diffuseAlbedo.a < 0.1f)
    {
        outAlpha = 0.0f;
        return float3(0.0f, 0.0f, 0.0f);
    }

    float3 bumpedNormal = normalW;
    if (matData.NormalMapIndex < NUM_TEXTURE)
    {
        //float3 normalMapSample = gTextureMapsLinear[matData.NormalMapIndex].SampleLevel(gsamAnisotropicWrap, tex, 0).rgb;
        //bumpedNormal = NormalSampleToWorldSpace(normalMapSample, normalW, tangent, matData.gNormalScale);

        uint tw, th, mips;
        gTextureMapsLinear[matData.NormalMapIndex].GetDimensions(0, tw, th, mips);
        float t = RayTCurrent();
        float tpm = TexelsPerMeter(uv0, uv1, uv2, p0, p1, p2, tw, th);
        float lod = ComputeLOD(t, tpm, mips);
        lod = clamp(lod - 1.25, 0.0f, min((float)mips - 1.0f, 4.0f));
        float3 normalMapSample = gTextureMapsLinear[matData.NormalMapIndex].SampleLevel(gsamAnisotropicWrap, tex, lod).rgb;
        bumpedNormal = NormalSampleToWorldSpace(normalMapSample, normalW, tangent, matData.gNormalScale);
    }

    float3 orm = float3(1.0f, 1.0f, 0.0f);
    if (matData.gORMIdx < NUM_TEXTURE)
    {
        //orm = gTextureMapsLinear[matData.gORMIdx].SampleLevel(gsamAnisotropicWrap, tex, 0).rgb;
        uint tw, th, mips;
        gTextureMapsLinear[matData.gORMIdx].GetDimensions(0, tw, th, mips);
        float t = RayTCurrent();
        float tpm = TexelsPerMeter(uv0, uv1, uv2, p0, p1, p2, tw, th);
        float lod = ComputeLOD(t, tpm, mips);
        lod = clamp(lod - 1.25, 0.0f, min((float)mips - 1.0f, 4.0f));
        orm = gTextureMapsLinear[matData.gORMIdx].SampleLevel(gsamAnisotropicWrap, tex, lod).rgb;
    }

    float ambientOcclusion = lerp(1.0f, orm.r, saturate(matData.gOcclusionStrength));
    //roughness = clamp(saturate(roughness * orm.g), 0.45f, 0.90f);
    roughness = saturate(roughness * orm.g);
    float metal = saturate(matData.gMetallic * orm.b);

    const float shininess = (1.0f - roughness) * (1.0f - roughness);
    fresnelR0 = lerp(fresnelR0, diffuseAlbedo.rgb, metal);
    diffuseAlbedo.rgb *= (1.0f - metal);

    float3 toEye = normalize(gEyePosW - posW);

    float shadow = 1.0f;
    if (payload.depth < MAX_RAY_DEPTH)
    {
        RayDesc shadowRay;
        shadowRay.Origin = posW + bumpedNormal * 0.005f;
        shadowRay.Direction = normalize(-gLights[0].Direction);
        shadowRay.TMin = 0.0f;
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
    float3 direct = ComputeLighting(gLights, mat, posW, bumpedNormal, toEye, float3(shadow, shadow, shadow)).rgb;

    float3 hemi = EvaluateEnvironment(bumpedNormal);
    float3 kS = lerp(float3(0.04f, 0.04f, 0.04f), matData.DiffuseAlbedo.rgb, metal);
    float3 kD = (1.0f - kS) * (1.0f - metal);
    float3 ambient = ambientOcclusion * kD * hemi;
    ambient *= lerp(1.0f, shadow, 0.7f);

    float3 emissive = matData.gEmissiveFactor;
    if (matData.gEmissiveIdx < NUM_TEXTURE)
    {
        //emissive *= gTextureMapsSRGB[matData.gEmissiveIdx].SampleLevel(gsamLinearWrap, tex, 0).rgb;
        uint tw, th, mips;
        gTextureMapsSRGB[matData.gEmissiveIdx].GetDimensions(0, tw, th, mips);
        float t = RayTCurrent();
        float tpm = TexelsPerMeter(uv0, uv1, uv2, p0, p1, p2, tw, th);
        float lod = ComputeLOD(t, tpm, mips);
        lod = clamp(lod - 1.25, 0.0f, min((float)mips - 1.0f, 4.0f));
        emissive *= gTextureMapsSRGB[matData.gEmissiveIdx].SampleLevel(gsamLinearWrap, tex, lod).rgb;
    }

    float3 lighting = ambient + direct + emissive * matData.gEmissiveStrength;

    outAlpha = diffuseAlbedo.a;
    return lighting;
}

[shader("raygeneration")]
void RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;

    float2 uv = (float2(pixel) + 0.5f) / float2(dim);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);

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
    payload.depth = 0;

    TraceRay(gScene, RAY_FLAG_NONE, 0xFF, 0, 2, 0, ray, payload);

    gOutput[pixel] = float4(payload.radiance, 1.0f);
}

[shader("miss")]
void Miss(inout RadiancePayload payload)
{
    payload.radiance = EvaluateEnvironment(-WorldRayDirection());
}

[shader("closesthit")]
void ClosestHit(inout RadiancePayload payload, Attributes attr)
{
    if (payload.depth >= MAX_RAY_DEPTH)
    {
        payload.radiance = 0.0f;
        return;
    }

    float alpha;
    float3 color = ShadeSurface(PrimitiveIndex(), attr.barycentrics, payload, alpha);
    payload.radiance = color;
    payload.depth++;
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