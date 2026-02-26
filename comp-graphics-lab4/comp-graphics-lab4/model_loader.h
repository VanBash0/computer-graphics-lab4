#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H

#include "assimp/scene.h"

#include "mesh_data.h"
#include "vertex.h"

#include <vector>
#include <string>

class ModelLoader {
public:
    ModelLoader(float scale = 1.f) : mScale(scale) {}

    std::vector<MeshData> loadModel(const std::string& fileName);

private:
    float mScale;

    DirectX::XMFLOAT3 transformVec(const aiVector3D& v, const aiMatrix4x4& m);
    void processNode(const aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform, std::vector<MeshData>& meshes);
    void processMesh(const aiMesh* mesh, const aiMatrix4x4& transform, MeshData& meshData, const aiScene* scene);
};

#endif // MODEL_LOADER_H