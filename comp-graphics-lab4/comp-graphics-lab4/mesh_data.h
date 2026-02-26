#ifndef MESH_DATA_H
#define MESH_DATA_H

#include "vertex.h"

#include <string>

struct MaterialData {
    DirectX::XMFLOAT4 diffuseColor = {1.f, 1.f, 1.f, 1.f};
    std::string diffuseTexturePath;
    int srvIndex = -1;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    MaterialData material;

    UINT startIndex = 0;
    UINT baseVertex = 0;
    UINT indexCount = 0;
};

#endif // MESH_DATA_H