#ifndef MESH_DATA_H
#define MESH_DATA_H

#include "vertex.h"

#include <SimpleMath.h>
#include <vector>

struct Material {
    Vector4 diffuseColor;
    Vector4 ambientColor;
    Vector4 specularColor;
    float shininess;
};

struct Submesh {
    UINT indexCount = 0;
    UINT startIndiceIndex = 0;
    UINT startVerticeIndex = 0;
    Material material;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;
};

#endif // MESH_DATA_H