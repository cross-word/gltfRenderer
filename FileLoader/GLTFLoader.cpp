#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
#include "GLTFLoader.h"
#include <filesystem>
#include <unordered_map>
#include <future>

static const XMMATRIX FlipZ = XMMatrixScaling(1.f, 1.f, -1.f);

const uint8_t* GetDataPtr(const tinygltf::Model& model, const tinygltf::Accessor& acc)
{
    const auto& bv = model.bufferViews[acc.bufferView];
    const auto& buf = model.buffers[bv.buffer];
    return buf.data.data() + bv.byteOffset + acc.byteOffset;
}

size_t CompSize(int componentType)
{
    switch (componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:         return 4;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:return 2;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return 1;
        default: return 4;
    }
}

size_t ElemCount(int type)
{
    switch (type)
    {
        case TINYGLTF_TYPE_SCALAR: return 1;
        case TINYGLTF_TYPE_VEC2:   return 2;
        case TINYGLTF_TYPE_VEC3:   return 3;
        case TINYGLTF_TYPE_VEC4:   return 4;
        default: return 0;
    }
}

size_t GetStride(const tinygltf::Model& m, const tinygltf::Accessor& acc)
{
    const auto& bv = m.bufferViews[acc.bufferView];
    size_t implicit = CompSize(acc.componentType) * ElemCount(acc.type);
    return bv.byteStride ? (size_t)bv.byteStride : implicit;
}

DXGI_FORMAT ToIndexFormat(const tinygltf::Accessor& acc)
{
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) return DXGI_FORMAT_R16_UINT;
    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)   return DXGI_FORMAT_R32_UINT;

    return DXGI_FORMAT_R32_UINT;
}

XMMATRIX NodeLocal(const tinygltf::Node& n)
{
    if (n.matrix.size() == 16)
    {
        XMMATRIX M = XMMatrixSet(
            (float)n.matrix[0], (float)n.matrix[1], (float)n.matrix[2], (float)n.matrix[3],
            (float)n.matrix[4], (float)n.matrix[5], (float)n.matrix[6], (float)n.matrix[7],
            (float)n.matrix[8], (float)n.matrix[9], (float)n.matrix[10], (float)n.matrix[11],
            (float)n.matrix[12], (float)n.matrix[13], (float)n.matrix[14], (float)n.matrix[15]);
        return XMMatrixTranspose(M);
    }
    else
    {
        XMVECTOR S = XMVectorSet(n.scale.size() ? (float)n.scale[0] : 1.f,
            n.scale.size() ? (float)n.scale[1] : 1.f,
            n.scale.size() ? (float)n.scale[2] : 1.f, 0);
        XMVECTOR R = XMQuaternionIdentity();
        if (n.rotation.size() == 4) R = XMVectorSet((float)n.rotation[0], (float)n.rotation[1], (float)n.rotation[2], (float)n.rotation[3]);
        XMVECTOR T = XMVectorSet(n.translation.size() ? (float)n.translation[0] : 0.f,
            n.translation.size() ? (float)n.translation[1] : 0.f,
            n.translation.size() ? (float)n.translation[2] : 0.f, 0);
        return XMMatrixAffineTransformation(S, XMVectorZero(), R, T);
    }
}

// gltf primitive → meshData
MeshData BuildMeshFromPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& prim)
{
    MeshData md;

    auto findAttr = [&](const char* key)->const tinygltf::Accessor*
    {
        auto it = prim.attributes.find(key);
        if (it == prim.attributes.end()) return nullptr;
        return &model.accessors[it->second];
    };

    const tinygltf::Accessor* posAcc = findAttr("POSITION");
    const tinygltf::Accessor* nrmAcc = findAttr("NORMAL");
    const tinygltf::Accessor* texAcc = findAttr("TEXCOORD_0");
    const tinygltf::Accessor* tanAcc = findAttr("TANGENT");

    const size_t vcount = posAcc ? posAcc->count : 0;
    md.vertices.reserve(vcount);

    auto readVec = [&](const tinygltf::Accessor* acc, size_t i, float* dst, int wantN)->bool
    {
        if (!acc) return false;
        const uint8_t* base = GetDataPtr(model, *acc);
        const size_t   stride = GetStride(model, *acc);
        const uint8_t* ptr = base + i * stride;

        switch (acc->componentType)
        {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
            {
                const float* f = reinterpret_cast<const float*>(ptr);
                for (int k = 0; k < wantN; k++) dst[k] = f[k];
                return true;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            {
                const uint16_t* s = reinterpret_cast<const uint16_t*>(ptr);
                const float sc = acc->normalized ? (1.0f / 65535.0f) : 1.0f;
                for (int k = 0; k < wantN; k++) dst[k] = s[k] * sc;
                return true;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            {
                const uint8_t* b = reinterpret_cast<const uint8_t*>(ptr);
                const float sc = acc->normalized ? (1.0f / 255.0f) : 1.0f;
                for (int k = 0; k < wantN; k++) dst[k] = b[k] * sc;
                return true;
            }
            default:
                return false;
        }
    };

    for (size_t i = 0; i < vcount; ++i)
    {
        Vertex v{};
        float tmp[4] = { 0,0,0,0 };

        if (posAcc) { readVec(posAcc, i, tmp, 3); v.position = { tmp[0], tmp[1], -tmp[2] }; }
        if (nrmAcc) { readVec(nrmAcc, i, tmp, 3); v.normal = { tmp[0], tmp[1], -tmp[2] }; }
        if (texAcc) { readVec(texAcc, i, tmp, 2); v.texC = { tmp[0], tmp[1] }; } // 필요 시 v = 1 - v[1]
        if (tanAcc) { float t[4] = { 0,0,0,1 }; readVec(tanAcc, i, t, 4); v.tangent = { t[0], t[1], -t[2], -t[3] }; }

        md.vertices.push_back(v);
    }

    // 인덱스
    const auto& idxAcc = model.accessors[prim.indices];
    const uint8_t* idxBase = GetDataPtr(model, idxAcc);
    size_t step = GetStride(model, idxAcc);
    if (step == 0) step = CompSize(idxAcc.componentType);

    for (size_t i = 0; i < idxAcc.count; ++i)
    {
        const uint8_t* p = idxBase + i * step;
        if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            md.indices.push_back(*(const uint16_t*)p);
        else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            md.indices.push_back(*(const uint32_t*)p);
        else
            md.indices.push_back(*(const uint8_t*)p);
    }
    if (prim.mode == TINYGLTF_MODE_TRIANGLES)
    {
        for (size_t i = 0; i + 2 < md.indices.size(); i += 3)
            std::swap(md.indices[i + 1], md.indices[i + 2]);
    }
    return md;
}

void BuildNodeWorlds(const tinygltf::Model& model,
    std::vector<XMFLOAT4X4>& nodeWorldOut)
{
    nodeWorldOut.assign(model.nodes.size(), XMFLOAT4X4{});
    std::function<void(int, XMMATRIX)> dfs = [&](int idx, XMMATRIX parent)
        {
            XMFLOAT4X4 lf; XMStoreFloat4x4(&lf, NodeLocal(model.nodes[idx]));
            XMMATRIX   lw = XMLoadFloat4x4(&lf);
            XMMATRIX   ww = XMMatrixMultiply(parent, lw);

            ww = FlipZ * ww * FlipZ; // RH→LH
            XMStoreFloat4x4(&nodeWorldOut[idx], ww);
            for (int c : model.nodes[idx].children) dfs(c, ww);
        };

    XMMATRIX I = XMMatrixIdentity();
    if (model.scenes.empty()) {
        for (size_t i = 0; i < model.nodes.size(); ++i) dfs((int)i, I);
    }
    else {
        int s = model.defaultScene >= 0 ? model.defaultScene : 0;
        for (int root : model.scenes[s].nodes) dfs(root, I);
    }
}

inline XMVECTOR NodeWorldForward(const XMMATRIX& M)
{
    return XMVector3Normalize(XMVector4Transform(XMVectorSet(0, 0, -1, 0), M));
}

inline XMVECTOR NodeWorldPosition(const XMMATRIX& M)
{
    return M.r[3]; // (x,y,z,1)
}

inline int LightTypeToInt(const std::string& t)
{
    if (t == "directional") return 0;
    if (t == "point")       return 1;
    return 2; // "spot"
}

void CollectGLTFLights(
    const tinygltf::Model& model,
    const std::vector<XMFLOAT4X4>& nodeWorldsLH,
    std::vector<Light>& out)
{
    out.clear();

    auto nodeLightIndex = [&](const tinygltf::Node& n)->int {
#ifdef HAS_TINYGLTF_NODE_LIGHT
        if (n.light >= 0) return n.light;
#endif
        auto it = n.extensions.find("KHR_lights_punctual");
        if (it != n.extensions.end() && it->second.IsObject())
        {
            const auto& obj = it->second.Get<tinygltf::Value::Object>();
            auto lit = obj.find("light");
            if (lit != obj.end())
            {
                if (lit->second.IsInt())    return lit->second.Get<int>();
                if (lit->second.IsNumber()) return (int)lit->second.GetNumberAsInt();
            }
        }
        return -1;
        };

    for (size_t ni = 0; ni < model.nodes.size(); ++ni)
    {
        int li = nodeLightIndex(model.nodes[ni]);
        if (li < 0 || li >= (int)model.lights.size()) continue;

        const auto& L = model.lights[li];
        Light g{};

        g.Color = { (float)L.color[0], (float)L.color[1], (float)L.color[2] }; //downcast to float
        g.Intensity = (L.intensity == 0.0) ? 5.5f : L.intensity;
        g.Type = LightTypeToInt(L.type);
        g.Range = (L.range > 0.0) ? (float)L.range : -1.0f;

        if (g.Type == LIGHT_TYPE_SPOT)
        {
            g.InnerCos = cosf((float)L.spot.innerConeAngle);
            g.OuterCos = cosf((float)L.spot.outerConeAngle);
        }
        else
        {
            g.InnerCos = 0.0f; g.OuterCos = -1.0f;
        }

        XMMATRIX W = XMLoadFloat4x4(&nodeWorldsLH[ni]);
        XMVECTOR dir = NodeWorldForward(W);
        XMVECTOR pos = NodeWorldPosition(W);

        XMStoreFloat3(&g.Direction, dir);
        XMStoreFloat3(&g.Position, pos);

        out.push_back(g);
    }
}

SceneData LoadGLTFScene(const std::wstring& filename)
{
    SceneData scene;

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    std::filesystem::path path(filename);
    std::string input = path.string();

    bool ok = false;
    if (path.extension() == L".gltf") ok = loader.LoadASCIIFromFile(&model, &err, &warn, input);
    else                              ok = loader.LoadBinaryFromFile(&model, &err, &warn, input);
    if (!ok) return scene;

    ///////////texture path
    auto base = std::filesystem::weakly_canonical(std::filesystem::absolute(path.parent_path()));
    scene.textures.reserve(model.images.size());
    for (const auto& img : model.images)
    {
        std::filesystem::path p = img.uri;
        auto abs = std::filesystem::weakly_canonical(base / p);
        scene.textures.push_back(abs.wstring());
    }

    ///////////material info
    scene.materials.reserve(model.materials.size());
    for (const auto& m : model.materials)
    {
        MaterialTex mt{};
        if (m.pbrMetallicRoughness.baseColorFactor.size() == 4)
        {
            mt.DiffuseAlbedo = XMFLOAT4(
                (float)m.pbrMetallicRoughness.baseColorFactor[0],
                (float)m.pbrMetallicRoughness.baseColorFactor[1],
                (float)m.pbrMetallicRoughness.baseColorFactor[2],
                (float)m.pbrMetallicRoughness.baseColorFactor[3]);
        }
        if (m.pbrMetallicRoughness.roughnessFactor > 0) mt.Roughness = (float)m.pbrMetallicRoughness.roughnessFactor;
        if (m.pbrMetallicRoughness.metallicFactor > 0)  mt.Metallic = (float)m.pbrMetallicRoughness.metallicFactor;
        if (m.normalTexture.scale > 0) mt.NormalScale = (float)m.normalTexture.scale;
        if (m.occlusionTexture.strength > 0) mt.OcclusionStrength = (float)m.occlusionTexture.strength;
        //if (m.extensions["KHR_materials_emissive_strength"])
        if (m.emissiveFactor.size() == 3)
        {
            mt.EmissiveFactor = XMFLOAT3(
                (float)m.emissiveFactor[0],
                (float)m.emissiveFactor[1],
                (float)m.emissiveFactor[2]);
        };

        if (m.pbrMetallicRoughness.baseColorTexture.index >= 0) mt.BaseColorIndex = m.pbrMetallicRoughness.baseColorTexture.index;
        if (m.normalTexture.index >= 0)                         mt.NormalIndex = m.normalTexture.index;
        if (m.occlusionTexture.index >= 0)                      mt.OcclusionIndex = m.occlusionTexture.index;
        if (m.emissiveTexture.index >= 0)                       mt.EmissiveIndex = m.emissiveTexture.index;
        if (m.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) mt.ORMIndex = m.pbrMetallicRoughness.metallicRoughnessTexture.index;

        scene.materials.push_back(mt);
    }

    ///////////mesh primi
    std::vector<std::future<PrimitiveMeshEx>> asyncPrimitive;
    const int primitiveNum =
        std::accumulate(model.meshes.begin(), model.meshes.end(), 0,
            [](int a, const auto& m) { return a + (int)m.primitives.size(); });

    asyncPrimitive.reserve(primitiveNum);
    for (size_t mi = 0; mi < model.meshes.size(); ++mi)
    {
        for (size_t pi = 0; pi < model.meshes[mi].primitives.size(); ++pi)
        {
            asyncPrimitive.emplace_back(std::async(std::launch::async, [&, mi, pi]() {
                const auto& mesh = model.meshes[mi];
                const auto& prim = mesh.primitives[pi];
                PrimitiveMeshEx tmpPM{};
                tmpPM.mesh = BuildMeshFromPrimitive(model, prim);
                tmpPM.material = prim.material;
                return tmpPM;
                }));
        }
    }
    scene.primitives.resize(primitiveNum);
    for (size_t i = 0; i < primitiveNum; ++i)
    {
        scene.primitives[i] = asyncPrimitive[i].get();
    }

    // 인스턴스(노드 트리 순회)
    // 노드의 mesh가 k번째 mesh이고 primitive pi라면, "mesh 앞의 프리미티브 개수 합 + pi" = scene.primitives 인덱스
    std::vector<int> meshPrimOffset(model.meshes.size(), 0);
    {
        int acc = 0;
        for (size_t mi = 0; mi < model.meshes.size(); ++mi)
        {
            meshPrimOffset[mi] = acc;
            acc += (int)model.meshes[mi].primitives.size();
        }
    }

    std::vector<std::pair<int, XMFLOAT4X4>> instRaw; // (primitiveIdx, world)
    XMMATRIX I = XMMatrixIdentity();
    if (model.scenes.empty())
    {
        // default scene = 0
        if (!model.nodes.empty())
        {
            for (size_t i = 0; i < model.nodes.size(); ++i)
            {
                std::function<void(int, const XMMATRIX&)> dfs = [&](int nodeIdx, const XMMATRIX& parent)
                    {
                        const auto& n = model.nodes[nodeIdx];
                        XMFLOAT4X4 lf;
                        XMStoreFloat4x4(&lf, NodeLocal(n));
                        XMMATRIX lw = XMLoadFloat4x4(&lf);
                        XMMATRIX ww = XMMatrixMultiply(parent, lw);
                        // RH→LH
                        ww = FlipZ * ww * FlipZ;
                        if (n.mesh >= 0)
                        {
                            const auto& mesh = model.meshes[n.mesh];
                            for (size_t pi = 0; pi < mesh.primitives.size(); ++pi)
                            {
                                int primIdx = meshPrimOffset[n.mesh] + (int)pi;
                                XMFLOAT4X4 W; XMStoreFloat4x4(&W, XMMatrixTranspose(ww));
                                instRaw.emplace_back(primIdx, W);
                            }
                        }
                        for (int c : n.children) dfs(c, ww);
                    };
                dfs((int)i, I);
            }
        }
    }
    else
    {
        int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
        const auto& sc = model.scenes[sceneIdx];
        for (int root : sc.nodes)
        {
            std::function<void(int, const XMMATRIX&)> dfs = [&](int nodeIdx, const XMMATRIX& parent)
                {
                    const auto& n = model.nodes[nodeIdx];
                    XMFLOAT4X4 lf;
                    XMStoreFloat4x4(&lf, NodeLocal(n));
                    XMMATRIX lw = XMLoadFloat4x4(&lf);
                    XMMATRIX ww = XMMatrixMultiply(parent, lw);
                    // RH→LH
                    ww = FlipZ * ww * FlipZ;
                    if (n.mesh >= 0)
                    {
                        const auto& mesh = model.meshes[n.mesh];
                        for (size_t pi = 0; pi < mesh.primitives.size(); ++pi)
                        {
                            int primIdx = meshPrimOffset[n.mesh] + (int)pi;
                            XMFLOAT4X4 W; XMStoreFloat4x4(&W, XMMatrixTranspose(ww));
                            instRaw.emplace_back(primIdx, W);
                        }
                    }
                    for (int c : n.children) dfs(c, ww);
                };
            dfs(root, I);
        }
    }

    scene.instances.reserve(instRaw.size());
    for (auto& pr : instRaw)
    {
        NodeInstance ni{};
        ni.primitive = pr.first;
        ni.world = pr.second;
        scene.instances.push_back(ni);
    }

    std::vector<XMFLOAT4X4> nodeWorldsLH;
    BuildNodeWorlds(model, nodeWorldsLH);

    CollectGLTFLights(model, nodeWorldsLH, scene.lights);

    return scene;
}