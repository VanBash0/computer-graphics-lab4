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

void ModelLoader::processMesh(const aiMesh* mesh, const aiMatrix4x4& transform, MeshData& meshData, const aiScene* scene) {
    if (mesh->mMaterialIndex <= -1) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        aiColor4D color;
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            meshData.material.diffuseColor = XMFLOAT4(color.r, color.g, color.b, color.a);
        }

        aiString texturePath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
            meshData.material.diffuseTexturePath = texturePath.C_Str();
        }
    }

    meshData.vertices.reserve(mesh->mNumVertices);
    meshData.indices.reserve(mesh->mNumFaces * 3);

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

            if (mesh->HasTextureCoords(0)) {
                vertex.texcoord.x = mesh->mTextureCoords[0][idx].x;
                vertex.texcoord.y = mesh->mTextureCoords[0][idx].y;
            }
            else {
                vertex.texcoord = { 0.f, 0.f };
            }

            meshData.vertices.push_back(vertex);
            meshData.indices.push_back(static_cast<uint32_t>(meshData.vertices.size() - 1));
        }
    }
}

void ModelLoader::processNode(const aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform, std::vector<MeshData>& meshes) {
    aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

    for (UINT i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        MeshData meshData;
        processMesh(mesh, globalTransform, meshData, scene);
        meshes.push_back(std::move(meshData));
    }

    for (UINT i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, globalTransform, meshes);
    }
}

std::vector<MeshData> ModelLoader::loadModel(const std::string& fileName) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(fileName,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_FlipUVs
    );

    std::vector<MeshData> meshes;
    aiMatrix4x4 identity;
    processNode(scene->mRootNode, scene, identity, meshes);
    return meshes;
}