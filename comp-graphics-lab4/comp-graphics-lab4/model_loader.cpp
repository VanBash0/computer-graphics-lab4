#include "model_loader.h"
#include "assimp/Importer.hpp"

#include <assimp/postprocess.h>
#include <DirectXMath.h>
#include <stdexcept>

using namespace DirectX;

static XMMATRIX aiToXM(const aiMatrix4x4& m) {
    return XMMATRIX(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

void ModelLoader::parseMesh(const aiMesh* mesh, const aiMatrix4x4& transform, MeshData& meshData) {
    UINT baseVertex = static_cast<UINT>(meshData.vertices.size());
    UINT startIndex = static_cast<UINT>(meshData.indices.size());

    XMMATRIX M = aiToXM(transform);
    XMMATRIX normalMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, M));

    for (UINT i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;

        XMVECTOR pos = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&mesh->mVertices[i]));

        pos = XMVector3Transform(pos, M);
        pos *= mScale;

        XMStoreFloat3(&vertex.position, pos);

        if (mesh->HasNormals()) {
            XMVECTOR normal = XMLoadFloat3(
                reinterpret_cast<const XMFLOAT3*>(&mesh->mNormals[i]));

            normal = XMVector3TransformNormal(normal, normalMatrix);
            normal = XMVector3Normalize(normal);

            XMStoreFloat3(&vertex.normal, normal);
        }
        else {
            vertex.normal = { 0.f, 1.f, 0.f };
        }
        meshData.vertices.push_back(vertex);
    }

    for (UINT i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];

        for (UINT j = 0; j < face.mNumIndices; ++j) {
            meshData.indices.push_back(baseVertex + face.mIndices[j]);
        }
    }

    Submesh submesh;
    submesh.indexCount = static_cast<UINT>(mesh->mNumFaces * 3);
    submesh.startIndiceIndex = startIndex;
    submesh.startVerticeIndex = baseVertex;
    meshData.submeshes.push_back(submesh);
}

void ModelLoader::parseNode(const aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform, MeshData& meshData) {
    aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

    for (UINT i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        parseMesh(mesh, globalTransform, meshData);
    }

    for (UINT i = 0; i < node->mNumChildren; ++i) {
        parseNode(node->mChildren[i], scene, globalTransform, meshData);
    }
}

MeshData ModelLoader::loadModel(const std::string& fileName) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        fileName,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality
    );

    if (!scene || !scene->mRootNode)
        throw std::runtime_error(importer.GetErrorString());

    MeshData meshData;
    aiMatrix4x4 identity;
    parseNode(scene->mRootNode, scene, identity, meshData);

    return meshData;
}