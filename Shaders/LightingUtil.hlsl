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

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float  Shininess;
};

// Schlick approximation of fresnel
float3 SchlickFresnel(float3 R0, float3 normal, float3 Light)
{
    float c = saturate(dot(normal, Light));
    float f0 = 1.0 - c;
    return R0 + (1.0 - R0) * (f0 * f0 * f0 * f0 * f0);
}

// return diffuse + specular
float3 BlinnPhong(float3 LIntensity, float3 Ldir, float3 normal, float3 eyeVector, Material mat)
{
    const float m = mat.Shininess * 256.0f;
    float3 Halfway = normalize(eyeVector + Ldir);
    float  roughFactor = (m + 8.0f) * pow(max(dot(Halfway, normal), 0.0f), m) / 8.0f; //surface roughness
    float3 Fresnel = SchlickFresnel(mat.FresnelR0, Halfway, Ldir); //fresnel effect
    float3 spec = Fresnel * roughFactor;
    //spec = spec / (spec + 1.0f); // LDR adjustment
    return (mat.DiffuseAlbedo.rgb + spec) * LIntensity;
}

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

// ------------------------------------------------------------
// Directional: E[lux] ≈ Intensity * N·L
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 eyeVector)
{
    float3 Ldir = -normalize(L.Direction);
    float  NdotL = max(dot(Ldir, normal), 0.0f);
    float3 LIntensity = L.Color * (L.Intensity * NdotL);
    return BlinnPhong(LIntensity, Ldir, normal, eyeVector, mat);
}

// Point: E ≈ I[cd] / d^2
float3 ComputePointLight(Light L, Material mat, float3 objectPosition, float3 normal, float3 eyeVector)
{
    float3 Ldir = L.Position - objectPosition;
    float  d = length(Ldir);
    if (L.Range > 0.0f && d > L.Range) return 0.0f; //out of range
    Ldir = Ldir / max(d, 1e-4f);

    float  NdotL = max(dot(Ldir, normal), 0.0f);
    float  att = DistanceAttenuation(d, L.Range); //attenuation
    float3 LIntensity = L.Color * (L.Intensity * NdotL * att);
    return BlinnPhong(LIntensity, Ldir, normal, eyeVector, mat);
}

// Spot: Point * SpotShape
float3 ComputeSpotLight(Light L, Material mat, float3 objectPosition, float3 normal, float3 eyeVector)
{
    float3 Ldir = L.Position - objectPosition;
    float  d = length(Ldir);
    if (L.Range > 0.0f && d > L.Range) return 0.0f;
    Ldir = Ldir / max(d, 1e-4f);

    float  NdotL = max(dot(Ldir, normal), 0.0f);
    float  attD = DistanceAttenuation(d, L.Range);

    float  cosTheta = dot(-Ldir, normalize(L.Direction));
    float  attS = SpotAttenuation(cosTheta, L.InnerCos, L.OuterCos);

    float3 LIntensity = L.Color * (L.Intensity * NdotL * attD * attS);
    return BlinnPhong(LIntensity, Ldir, normal, eyeVector, mat);
}

// ------------------------------------------------------------
float4 ComputeLighting(Light gLights[NUM_LIGHTS], Material mat,
    float3 objectPosition, float3 normal, float3 toEye, float3 shadowFactor)
{
    float3 sumL = 0.0f;
    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    [unroll]
        for (i = 0; i < NUM_DIR_LIGHTS; ++i)
            sumL += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
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

    return float4(sumL, 0.0f);
}
