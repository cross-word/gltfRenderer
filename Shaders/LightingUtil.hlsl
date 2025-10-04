//***************************************************************************************
// LightingUtil.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Contains API for shader lighting.
//***************************************************************************************

#ifndef NUM_LIGHTS
#define NUM_LIGHTS 25
#endif
struct Light
{
    float3 Color;   float Intensity;   // dir: lux, point/spot: cd
    float3 Direction; float Range;     // <=0: infinite
    float3 Position;  float InnerCos;  // spot only
    int    Type;     float OuterCos;   // spot only
    float2 _pad_;
};
static const float PI = 3.14159265f;
struct PbrMaterial
{
    float3 BaseColor;
    float3 F0;
    float  Roughness;
    float  Metallic;
};

// smooth attenuation of light intensity
float DistanceAttenuation(float d, float range)
{
    float invSq = 1.0f / max(d * d, 1e-4f);
    if (range <= 0.0f) return invSq;
    float x = saturate(d / range);
    float smooth = (1.0f - x * x);
    return invSq * smooth * smooth;
}

// angle attenuation of light intensity
float SpotAttenuation(float cosTheta, float innerCos, float outerCos)
{
    float t = saturate((cosTheta - outerCos) / max(1e-4f, (innerCos - outerCos)));
    return t * t;
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float denom = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * denom * denom, 1e-4f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) * 0.125f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    float f = saturate(1.0f - cosTheta);
    float factor = f * f * f * f * f;
    return F0 + (1.0f - F0) * factor;
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float f = saturate(1.0f - cosTheta);
    float factor = f * f * f * f * f;
    float3 maxF = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (maxF - F0) * factor;
}

float3 EvaluatePBR(float3 radiance, float3 L, float3 normal, float3 viewDir, PbrMaterial mat)
{
    float NdotL = saturate(dot(normal, L));
    if (NdotL <= 0.0f)
        return 0.0f;

    float3 H = normalize(viewDir + L);
    float3 F = FresnelSchlick(saturate(dot(H, viewDir)), mat.F0);
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - mat.Metallic);

    float NDF = DistributionGGX(normal, H, mat.Roughness);
    float G = GeometrySmith(normal, viewDir, L, mat.Roughness);
    float NdotV = saturate(dot(normal, viewDir));
    float denom = max(4.0f * NdotV * NdotL, 1e-4f);
    float3 specular = (NDF * G * F) / denom;
    float3 diffuse = kD * mat.BaseColor / PI;

    return (diffuse + specular) * radiance * NdotL;
}


// ------------------------------------------------------------
// Directional: E[lux] ≈ Intensity * N·L
float3 ComputeDirectionalLight(Light L, PbrMaterial mat, float3 normal, float3 eyeVector)
{
    float3 Ldir = -normalize(L.Direction);
    float3 radiance = L.Color * L.Intensity;
    return EvaluatePBR(radiance, Ldir, normal, eyeVector, mat);
}

// Point: E ≈ I[cd] / d^2
float3 ComputePointLight(Light L, PbrMaterial mat, float3 objectPosition, float3 normal, float3 eyeVector)
{
    float3 Ldir = L.Position - objectPosition;
    float  d = length(Ldir);
    if (L.Range > 0.0f && d > L.Range) return 0.0f; //out of range
    Ldir = Ldir / max(d, 1e-4f);

    float  att = DistanceAttenuation(d, L.Range); //attenuation
    float3 radiance = L.Color * (L.Intensity * att);
    return EvaluatePBR(radiance, Ldir, normal, eyeVector, mat);
}

// Spot: Point * SpotShape
float3 ComputeSpotLight(Light L, PbrMaterial mat, float3 objectPosition, float3 normal, float3 eyeVector)
{
    float3 Ldir = L.Position - objectPosition;
    float  d = length(Ldir);
    if (L.Range > 0.0f && d > L.Range) return 0.0f;
    Ldir = Ldir / max(d, 1e-4f);

    float  attD = DistanceAttenuation(d, L.Range);

    float  cosTheta = dot(-Ldir, normalize(L.Direction));
    float  attS = SpotAttenuation(cosTheta, L.InnerCos, L.OuterCos);

    float3 radiance = L.Color * (L.Intensity * attD * attS);
    return EvaluatePBR(radiance, Ldir, normal, eyeVector, mat);
}

// ------------------------------------------------------------
float3 ComputeLighting(Light gLights[NUM_LIGHTS], PbrMaterial mat,
    float3 objectPosition, float3 normal, float3 toEye, float shadowFactor)
{
    float3 sumL = 0.0f;
    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    [unroll]
        for (i = 0; i < NUM_DIR_LIGHTS; ++i)
        {
            float visibility = (i == 0) ? shadowFactor : 1.0f;
            sumL += visibility * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
        }
#endif

#if (NUM_POINT_LIGHTS > 0)
    [unroll]
        for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
            sumL += ComputePointLight(gLights[i], mat, objectPosition, normal, toEye);
#endif

#if (NUM_SPOT_LIGHTS > 0)
    [unroll]
        for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
            sumL += ComputeSpotLight(gLights[i], mat, objectPosition, normal, toEye);
#endif

    return sumL;
}
