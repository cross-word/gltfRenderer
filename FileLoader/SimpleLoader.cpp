#include "SimpleLoader.h"
#include <DirectXColors.h>
#include <fstream>
#include <sstream>

MeshData LoadOBJ(const std::wstring& filename) {
    MeshData mesh;
    std::wifstream file(filename);
    if (!file.is_open()) {
        return mesh;
    }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> colors;
    std::vector<XMFLOAT2> texCode;
    std::vector<XMFLOAT4> tanU;

    std::wstring line;
    while (std::getline(file, line)) {
        std::wistringstream iss(line);
        std::wstring prefix;
        iss >> prefix;
        if (prefix == L"v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.emplace_back(x, y, z);
            float b, g, r;
            iss >> b >> g >> r;
            colors.emplace_back(b, g, r);
            float tex_x, tex_y;
            iss >> tex_x >> tex_y;
            texCode.emplace_back(tex_x, tex_y);
            float tan_x, tan_y, tan_z, tan_w;
            iss >> tan_x >> tan_y >> tan_z >> tan_w;
            tanU.emplace_back(tan_x, tan_y, tan_z, tan_w);
        }
        else if (prefix == L"f") {
            std::wstring v1, v2, v3;
            iss >> v1 >> v2 >> v3;
            auto parseIndex = [](const std::wstring& token) {
                size_t pos = token.find(L"/");
                return static_cast<uint32_t>(std::stoi(token.substr(0, pos))) - 1;
                };
            uint32_t i1 = parseIndex(v1);
            uint32_t i2 = parseIndex(v2);
            uint32_t i3 = parseIndex(v3);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }

    for (int i = 0; i < positions.size(); i++) {
        Vertex v;
        v.position = positions[i];
        v.normal = colors[i];
        v.texC = texCode[i];
        v.tangent = tanU[i];
        mesh.vertices.push_back(v);
    }
    return mesh;
}