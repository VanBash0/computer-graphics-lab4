#include "model_loader.h"
#include "assimp/Importer.hpp"

#include <assimp/postprocess.h>
#include <stdexcept>

using namespace DirectX;

DirectX::XMFLOAT3 ModelLoader::transformVec(const aiVector3D& v, const aiMatrix4x4& m) {
    float x = m.a1 * v.x + m.a2 * v.y + m.a3 * v.z + m.a4;
    float y = m.b1 * v.x + m.b2 * v.y + m.b3 * v.z + m.b4;
    float z = m.c1 * v.x + m.c2 * v.y + m.c3 * v.z + m.c4;
    return { x * mScale, y * mScale, z * mScale };
}

void ModelLoader::processMesh(const aiMesh* mesh, const aiMatrix4x4& transform, MeshData& meshData) {
    for (UINT32 i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        for (UINT32 j = 0; j < face.mNumIndices; ++j) {
            UINT idx = face.mIndices[j];
            Vertex vertex;

            vertex.position = transformVec(mesh->mVertices[idx], transform);
            if (mesh->HasNormals() && mesh->mNormals) {
                aiMatrix4x4 rotMat = transform;
                rotMat.a4 = rotMat.b4 = rotMat.c4 = 0;
                XMFLOAT3 n = transformVec(mesh->mNormals[idx], rotMat);
                XMVECTOR nv = XMLoadFloat3(&n);
                nv = XMVector3Normalize(nv);
                XMStoreFloat3(&vertex.normal, nv);
            }
            else {
                vertex.normal = { 0.f, 1.f, 0.f };
            }

            meshData.vertices.push_back(vertex);
            meshData.indices.push_back(static_cast<uint32_t>(meshData.vertices.size() - 1));
        }
    }
}

void ModelLoader::processNode(const aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform, MeshData& meshData) {
    aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

    for (UINT i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        processMesh(mesh, globalTransform, meshData);
    }

    for (UINT i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, globalTransform, meshData);
    }
}

MeshData ModelLoader::loadModel(const std::string& fileName) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fileName,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality
    );

    MeshData meshData;
    aiMatrix4x4 identity;
    processNode(scene->mRootNode, scene, identity, meshData);
    return meshData;
}