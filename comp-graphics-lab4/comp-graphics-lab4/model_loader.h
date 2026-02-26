#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include "assimp/scene.h"
#include "vertex.h"

#include <vector>
#include <string>

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

class ModelLoader {
public:
    ModelLoader(float scale = 1.f) : mScale(scale) {}

    MeshData loadModel(const std::string& fileName);

private:
    float mScale;

    DirectX::XMFLOAT3 transformVec(const aiVector3D& v, const aiMatrix4x4& m);
    void processNode(const aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform, MeshData& meshData);
    void processMesh(const aiMesh* mesh, const aiMatrix4x4& transform, MeshData& meshData);
};

#endif // MODEL_LOADER_H