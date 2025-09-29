#pragma once

#include <string>
#include <vector>
#include <DirectXMath.h>
using namespace DirectX;

#ifdef _ENABLE_FILE_LOAD
#define FileLoad_API __declspec(dllexport)
#else
#define FileLoad_API __declspec(dllimport)
#endif

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 texC;
    XMFLOAT4 tangent;
};

struct MeshData
{
    ~MeshData() = default;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

FileLoad_API MeshData LoadOBJ(const std::wstring& filename);