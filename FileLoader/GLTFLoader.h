#pragma once

#include "SimpleLoader.h"
#include <string>
#include <vector>
#include <DirectXMath.h>
#include <dxgiformat.h>

using namespace DirectX;
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT       1
#define LIGHT_TYPE_SPOT        2

#ifdef _ENABLE_FILE_LOAD
#  define FileLoad_API __declspec(dllexport)
#else
#  define FileLoad_API __declspec(dllimport)
#endif


struct MaterialTex 
{
    XMFLOAT4 DiffuseAlbedo = { 1,1,1,1 }; // = pbrMetallicRoughness.baseColorFactor(vec4)
    XMFLOAT3 FresnelR0 = { 0.04f,0.04f,0.04f };
    float              Roughness = 1.0f;      // pbrMetallicRoughness.roughnessFactor

    // PBR
    float Metallic = 1.0f;        // pbrMetallicRoughness.metallicFactor
    float NormalScale = 1.0f;        // normalTexture.scale
    float OcclusionStrength = 1.0f;        // occlusionTexture.strength
    float EmissiveStrength = 1.0f;        // extensions["KHR_materials_emissive_strength"]

    XMFLOAT3 EmissiveFactor = { 0,0,0 }; // emissiveFactor(vec3)

    uint32_t BaseColorIndex = UINT32_MAX; // pbrMetallicRoughness.baseColorTexture.index + .texCoord
    uint32_t NormalIndex = UINT32_MAX; // normalTexture.index + .texCoord
    uint32_t ORMIndex = UINT32_MAX; // pbrMetallicRoughness.metallicRoughnessTexture.index + .texCoord
    uint32_t OcclusionIndex = UINT32_MAX;
    uint32_t EmissiveIndex = UINT32_MAX; // emissiveTexture.index
};

struct PrimitiveMeshEx
{
    MeshData mesh;
    int material = -1;
};

struct NodeInstance
{
    int primitive;
    XMFLOAT4X4 world;
};

struct Light
{
    XMFLOAT3 Color;     float Intensity; // dir: lux, pt/spot: cd
    XMFLOAT3 Direction; float Range;     // meters, <=0 infinite
    XMFLOAT3 Position;  float InnerCos;  // spot only
    int      Type;      float OuterCos;  // spot only
    float              _pad_[2];                  // 16B align
};
static_assert(sizeof(Light) % 16 == 0, "CB alignment");

struct SceneData
{
    std::vector<PrimitiveMeshEx> primitives;
    std::vector<NodeInstance>    instances;
    std::vector<MaterialTex>     materials;
    std::vector<std::wstring>    textures;
    std::vector<Light> lights;
};

FileLoad_API SceneData LoadGLTFScene(const std::wstring& filename);