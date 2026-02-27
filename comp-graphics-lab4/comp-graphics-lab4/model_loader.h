#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include "assimp/scene.h"
#include "vertex.h"

#include <vector>
#include <string>

struct Submesh {
    UINT indexCount = 0;
    UINT startIndiceIndex = 0;
    UINT startVerticeIndex = 0;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Submesh> submeshes;
};

class ModelLoader {
public:
    ModelLoader(float scale = 1.f) : mScale(scale) {}

    MeshData loadModel(const std::string& fileName);

private:
    float mScale;

    DirectX::XMFLOAT3 transformVec(const aiVector3D& v, const aiMatrix4x4& m);
    void parseNode(const aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform, MeshData& meshData);
    void parseMesh(const aiMesh* mesh, const aiMatrix4x4& transform, MeshData& meshData);
};

#endif // MODEL_LOADER_H