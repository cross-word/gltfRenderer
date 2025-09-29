#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <wrl.h>
#include <DirectXMath.h>
#include "../MiniEngineCore/EngineConfig.h"
#include "../FileLoader/GLTFLoader.h"
using namespace DirectX;

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif

enum class EViewType
{
    EVertexView,
    EIndexView,
    EConstantBufferView,
    EShaderResourceView,
    EUnorderedAccessView,
    EDepthStencilView,
    ERenderTargetView
};

#define MaxLights 25
static XMFLOAT4X4 XMMatIdentity()
{
    return XMFLOAT4X4({ 1, 0, 0, 0,
                        0, 1, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1 });
}

struct PassConstants // to slot b0 (per camera)
{
    XMFLOAT4X4 View = XMMatIdentity();
    XMFLOAT4X4 InvView = XMMatIdentity();
    XMFLOAT4X4 Proj = XMMatIdentity();
    XMFLOAT4X4 InvProj = XMMatIdentity();
    XMFLOAT4X4 ViewProj = XMMatIdentity();
    XMFLOAT4X4 InvViewProj = XMMatIdentity();

    XMFLOAT3 EyePosW = XMFLOAT3{ 0.0f, 0.0f, 0.0f };
    float cbPerObjectPad1 = 0.0f;
    XMFLOAT2 RenderTargetSize = XMFLOAT2{ 0.0f, 0.0f };
    XMFLOAT2 InvRenderTargetSize = XMFLOAT2{ 0.0f, 0.0f };
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    float TotalTime = 0.0f;
    float DeltaTime = 0.0f;

    XMFLOAT4 AmbientLight = XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f };

    XMFLOAT4X4 LightViewProj;
    XMFLOAT2   ShadowTexelSize; float _pad1[2];

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light Lights[MaxLights];
};

struct ObjectConstants
{
    XMFLOAT4X4 World = XMMatIdentity();
    XMFLOAT4X4 WorldInverseTranspose = XMMatIdentity();
    XMFLOAT4X4 TexTransform = XMMatIdentity();
};

struct MaterialConstants
{
    XMFLOAT4 DiffuseAlbedo = { 1,1,1,1 }; // = baseColorFactor
    XMFLOAT3 FresnelR0 = { 0.04f,0.04f,0.04f };
    float              Roughness = 1.0f;      // perceptual roughness [0..1]

    XMFLOAT4X4 MatTransform = XMMatIdentity(); // baseColor

    float Metallic = 1.0f;        // metallicFactor
    float NormalScale = 1.0f;        // normalTexture.scale
    float OcclusionStrength = 1.0f;        // occlusionTexture.strength
    float EmissiveStrength = 1.0f;        // KHR_materials_emissive_strength

    XMFLOAT3 EmissiveFactor = { 0,0,0 };
    //float _pad0 = 0.f;

    uint32_t BaseColorIndex = UINT32_MAX;
    uint32_t NormalIndex = UINT32_MAX;
    uint32_t ORMIndex = UINT32_MAX;
    uint32_t OcclusionIndex = UINT32_MAX;
    uint32_t EmissiveIndex = UINT32_MAX;
};
static_assert(sizeof(MaterialConstants) % 16 == 0, "CB/SSBO align");

struct UseFlags { bool SRGB = false, Linear = false; };
struct Material // to slot b2 (per object)
{
    // Unique material name for lookup.
    std::string Name;

    // Index into constant buffer corresponding to this material.
    int MatCBIndex = -1;

    // Index into SRV heap for diffuse texture.
    int DiffuseSrvHeapIndex = -1;

    // Index into SRV heap for normal texture.
    int NormalSrvHeapIndex = -1;

    // Dirty flag indicating the material has changed and we need to update the constant buffer.
    // Because we have a material constant buffer for each FrameResource, we have to apply the
    // update to each FrameResource.  Thus, when we modify a material we should set 
    // NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
    int NumFramesDirty = EngineConfig::SwapChainBufferCount;

    //UseFlags useFlag;

    MaterialConstants matConstant;
};

static UINT CalcConstantBufferByteSize(UINT byteSize)
{
    // Constant buffers must be a multiple of the minimum hardware
    // allocation size (usually 256 bytes).  So round up to nearest
    // multiple of 256.  We do this by adding 255 and then masking off
    // the lower 2 bytes which store all bits < 256.
    // Example: Suppose byteSize = 300.
    // (300 + 255) & ~255
    // 555 & ~255
    // 0x022B & ~0x00ff
    // 0x022B & 0xff00
    // 0x0200
    // 512
    return (byteSize + 255) & ~255;
}

struct HeapSlice
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle{};
};


static void GetFrustumCornersWS(const XMMATRIX& invViewProj, XMVECTOR outCorners[8])
{
    // NDC
    const XMFLOAT3 ndcPts[8] = {
        {-1,-1,0}, {+1,-1,0}, {-1,+1,0}, {+1,+1,0}, // near
        {-1,-1,1}, {+1,-1,1}, {-1,+1,1}, {+1,+1,1}  // far
    };

    for (int i = 0; i < 8; ++i)
    {
        XMVECTOR p = XMVectorSet(ndcPts[i].x, ndcPts[i].y, ndcPts[i].z, 1.0f);
        XMVECTOR w = XMVector3TransformCoord(p, invViewProj);
        outCorners[i] = w;
    }
}

static void BuildDirLightViewProj(
    FXMVECTOR lightDirWS,
    const XMMATRIX& invCameraViewProj,
    float shadowMapWidth,
    float shadowMapHeight,
    XMMATRIX& outLightView,
    XMMATRIX& outLightProj)
{
    // camera frustum
    XMVECTOR cornersWS[8];
    GetFrustumCornersWS(invCameraViewProj, cornersWS);

    XMVECTOR center = XMVectorZero();
    for (int i = 0; i < 8; ++i) center = XMVectorAdd(center, cornersWS[i]);
    center = XMVectorScale(center, 1.0f / 8.0f);

    float maxRadius = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        float d = XMVectorGetX(XMVector3Length(XMVectorSubtract(cornersWS[i], center)));
        maxRadius = max(maxRadius, d);
    }

    XMVECTOR L = XMVector3Normalize(lightDirWS);// light direction
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    if (fabsf(XMVectorGetX(XMVector3Dot(up, L))) > 0.99f)
        up = XMVectorSet(0, 0, 1, 0);

    XMVECTOR eye = XMVectorSubtract(center, XMVectorScale(L, maxRadius + 10.0f));
    outLightView = XMMatrixLookAtLH(eye, center, up);

    // map corners to light-view space
    XMVECTOR minPt = XMVectorSet(+FLT_MAX, +FLT_MAX, +FLT_MAX, 1);
    XMVECTOR maxPt = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1);
    for (int i = 0; i < 8; ++i)
    {
        XMVECTOR p = XMVector3TransformCoord(cornersWS[i], outLightView);
        minPt = XMVectorMin(minPt, p);
        maxPt = XMVectorMax(maxPt, p);
    }

    // 5) Off-center Ortho (LH)
    XMFLOAT3 minPtF, maxPtF;
    XMStoreFloat3(&minPtF, minPt);
    XMStoreFloat3(&maxPtF, maxPt);

    XMFLOAT3 centerLS{
        0.5f * (minPtF.x + maxPtF.x),
        0.5f * (minPtF.y + maxPtF.y),
        0.5f * (minPtF.z + maxPtF.z)
    };

    float width = maxPtF.x - minPtF.x;
    float height = maxPtF.y - minPtF.y;
    float depth = maxPtF.z - minPtF.z;

    const float maxXYExtent = max(width, height);
    const float xyPad = max(0.05f * maxXYExtent, 5.0f);
    const float zPad = max(0.25f * depth, 10.0f);

    width += 2.0f * xyPad;
    height += 2.0f * xyPad;
    depth += 2.0f * zPad;

    const float invShadowMapWidth = shadowMapWidth > 0 ? 1.0f / static_cast<float>(shadowMapWidth) : 0.0f;
    const float invShadowMapHeight = shadowMapHeight > 0 ? 1.0f / static_cast<float>(shadowMapHeight) : 0.0f;
    const float worldUnitsPerTexel = max(width * invShadowMapWidth, height * invShadowMapHeight);

    if (worldUnitsPerTexel > 0.0f)
    {
        centerLS.x = std::floor(centerLS.x / worldUnitsPerTexel) * worldUnitsPerTexel;
        centerLS.y = std::floor(centerLS.y / worldUnitsPerTexel) * worldUnitsPerTexel;
    }

    const float minX = centerLS.x - width * 0.5f;
    const float maxX = centerLS.x + width * 0.5f;
    const float minY = centerLS.y - height * 0.5f;
    const float maxY = centerLS.y + height * 0.5f;
    const float minZ = centerLS.z - depth * 0.5f;
    const float maxZ = centerLS.z + depth * 0.5f;

    outLightProj = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);
}

//check size_t to uint
inline UINT SizeToU32(size_t n)
{
    assert(n <= (std::numeric_limits<UINT>::max)());
    return static_cast<UINT>(n);
}