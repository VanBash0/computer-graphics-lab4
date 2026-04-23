#include "box_app.h"
#include "fail_checker.h"
#include "model_loader.h"
#include "DDSTextureLoader.h"
#include "rendering_system.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace {
    struct ParticleData {
        XMFLOAT3 position;
        float age;
        XMFLOAT3 velocity;
        float lifetime;
        XMFLOAT4 color;
        float size;
        XMFLOAT3 padding;
    };

    bool isColumnSubmesh(const Submesh& submesh) {
        return submesh.material.diffuseTextureName.find("column") != std::string::npos;
    }

    bool hasDisplacementTexture(const Submesh& submesh) {
        return submesh.material.displacementTextureName.find("Earth_") != std::string::npos;
    }

    void transformMesh(MeshData& mesh, float scale, const XMFLOAT3& offset) {
        for (auto& vertex : mesh.vertices) {
            vertex.position.x = vertex.position.x * scale + offset.x;
            vertex.position.y = vertex.position.y * scale + offset.y;
            vertex.position.z = vertex.position.z * scale + offset.z;
        }
    }

    void rotateMeshX(MeshData& mesh, float angleRadians) {
        const float cosAngle = std::cos(angleRadians);
        const float sinAngle = std::sin(angleRadians);

        for (auto& vertex : mesh.vertices) {
            const float y = vertex.position.y;
            const float z = vertex.position.z;
            vertex.position.y = y * cosAngle - z * sinAngle;
            vertex.position.z = y * sinAngle + z * cosAngle;

            const float ny = vertex.normal.y;
            const float nz = vertex.normal.z;
            vertex.normal.y = ny * cosAngle - nz * sinAngle;
            vertex.normal.z = ny * sinAngle + nz * cosAngle;

            const float ty = vertex.tangent.y;
            const float tz = vertex.tangent.z;
            vertex.tangent.y = ty * cosAngle - tz * sinAngle;
            vertex.tangent.z = ty * sinAngle + tz * cosAngle;

            const float by = vertex.bitangent.y;
            const float bz = vertex.bitangent.z;
            vertex.bitangent.y = by * cosAngle - bz * sinAngle;
            vertex.bitangent.z = by * sinAngle + bz * cosAngle;
        }
    }

    void appendMesh(MeshData& destination, const MeshData& source, float maxTessFactor) {
        const UINT vertexOffset = static_cast<UINT>(destination.vertices.size());
        const UINT indexOffset = static_cast<UINT>(destination.indices.size());

        destination.vertices.insert(destination.vertices.end(), source.vertices.begin(), source.vertices.end());

        destination.indices.reserve(destination.indices.size() + source.indices.size());
        for (uint32_t index : source.indices) {
            destination.indices.push_back(vertexOffset + index);
        }

        destination.submeshes.reserve(destination.submeshes.size() + source.submeshes.size());
        for (const auto& sourceSubmesh : source.submeshes) {
            Submesh submesh(sourceSubmesh);
            submesh.startIndiceIndex += indexOffset;
            submesh.startVerticeIndex += vertexOffset;
            submesh.maxTessellationFactor = maxTessFactor;
            destination.submeshes.push_back(submesh);
        }
    }
}

void BoxApp::buildResources() {
    initializeConstants();

    failCheck(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
    buildBuffers();
    loadTextures();
    buildRootSignature();
    buildParticleRootSignature();
    buildParticleComputeRootSignature();
    buildLightingRootSignature();
    buildConstantBuffer();
    buildParticleResources();
    createDefaultTextures();
    buildCbvSrvHeap();
    buildParticleDescriptors();
    bindMaterialsToTextures();
    buildPso(L"main_shader.hlsl", mPSO);
    buildPso(L"main_shader.hlsl", mEarthTessPSO, true);
    buildPso(L"column_shader.hlsl", mColumnPSO);
    buildPso(L"lighting_shader.hlsl", mLightingPSO);
    buildParticlePso();
    failCheck(mCommandList->Close());

    ID3D12CommandList* cmds[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(1, cmds);
    flushCommandQueue();
}

void BoxApp::setObjectSize(Vertex& vertex, float scale) {
    vertex.position.x *= scale;
    vertex.position.y *= scale;
    vertex.position.z *= scale;
}

void BoxApp::buildBuffers() {
    ModelLoader loader(SPONZA_SCALE);
    MeshData mesh = loader.loadModel("sponza.obj");

    ModelLoader earthLoader(1.0f);
    MeshData earthMesh = earthLoader.loadModel("Earth.fbx");
    rotateMeshX(earthMesh, XM_PI);
    transformMesh(earthMesh, EARTH_SCALE, XMFLOAT3(0.f, 0.f, 0.f));
    appendMesh(mesh, earthMesh, 10.f);

    MeshData billboardMesh;
    billboardMesh.vertices.resize(4);
    billboardMesh.vertices[0] = { {-BILLBOARD_SIZE, -BILLBOARD_SIZE, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f} };
    billboardMesh.vertices[1] = { {-BILLBOARD_SIZE,  BILLBOARD_SIZE, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f} };
    billboardMesh.vertices[2] = { { BILLBOARD_SIZE,  BILLBOARD_SIZE, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f} };
    billboardMesh.vertices[3] = { { BILLBOARD_SIZE, -BILLBOARD_SIZE, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f} };
    billboardMesh.indices = { 0, 1, 2, 0, 2, 3 };

    Submesh bmSubmesh;
    bmSubmesh.startVerticeIndex = 0;
    bmSubmesh.startIndiceIndex = 0;
    bmSubmesh.indexCount = 6;
    bmSubmesh.material.diffuseTextureName = "billboard";
    billboardMesh.submeshes.push_back(bmSubmesh);

    appendMesh(mesh, billboardMesh, 1.0f);

    const UINT vbByteSize = static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex));
    const UINT ibByteSize = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));

    mVertexBufferGPU = D3DUtil::createDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
        mesh.vertices.data(), vbByteSize, mVertexBufferUploader);

    mIndexBufferGPU = D3DUtil::createDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
        mesh.indices.data(), ibByteSize, mIndexBufferUploader);

    mSubmeshes.clear();
    mSubmeshWorlds.clear();
    std::vector<Submesh> earthSubmeshTemplates;
    earthSubmeshTemplates.reserve(earthMesh.submeshes.size());

    for (size_t i = 0; i < mesh.submeshes.size(); ++i) {
        const auto& sm = mesh.submeshes[i];
        Submesh submesh(sm);

        const UINT nextSubmeshStartVertex = (i + 1 < mesh.submeshes.size())
            ? mesh.submeshes[i + 1].startVerticeIndex
            : static_cast<UINT>(mesh.vertices.size());
        const UINT submeshVertexCount = nextSubmeshStartVertex - submesh.startVerticeIndex;
        const Vertex* submeshVertices = mesh.vertices.data() + submesh.startVerticeIndex;
        BoundingBox::CreateFromPoints(submesh.bounds, submeshVertexCount,
            &submeshVertices[0].position, sizeof(Vertex));

        if (hasDisplacementTexture(submesh)) {
            earthSubmeshTemplates.push_back(submesh);
            continue;
        }

        bool isBillboard = (submesh.material.diffuseTextureName == "billboard");
        if (isBillboard) {
            submesh.bounds.Center = mEarthPosition;
            submesh.bounds.Extents = XMFLOAT3(BILLBOARD_SIZE * 1.5f, BILLBOARD_SIZE * 1.5f, BILLBOARD_SIZE * 1.5f);
            mBillboardIndex = mSubmeshes.size();
        }

        mSubmeshes.push_back(submesh);
        XMFLOAT4X4 identity;
        XMStoreFloat4x4(&identity, XMMatrixIdentity());
        mSubmeshWorlds.push_back(identity);
    }

    mEarthSubmeshIndices.clear();
    mEarthSubmeshIndices.reserve(earthSubmeshTemplates.size());

    const XMMATRIX earthWorldMatrix = XMMatrixTranslation(mEarthPosition.x, mEarthPosition.y, mEarthPosition.z);
    XMFLOAT4X4 earthWorldTransform;
    XMStoreFloat4x4(&earthWorldTransform, earthWorldMatrix);

    for (const auto& earthTemplate : earthSubmeshTemplates) {
        Submesh earthSubmesh(earthTemplate);
        earthTemplate.bounds.Transform(earthSubmesh.bounds, earthWorldMatrix);
        earthSubmesh.maxTessellationFactor = 1.0f;
        mEarthSubmeshIndices.push_back(mSubmeshes.size());
        mSubmeshes.push_back(earthSubmesh);
        mSubmeshWorlds.push_back(earthWorldTransform);
    }

    buildOctree();

    mInputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    mVertexBufferView.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
    mVertexBufferView.StrideInBytes = sizeof(Vertex);
    mVertexBufferView.SizeInBytes = vbByteSize;

    mIndexBufferView.BufferLocation = mIndexBufferGPU->GetGPUVirtualAddress();
    mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    mIndexBufferView.SizeInBytes = ibByteSize;

    mIndexCount = static_cast<UINT>(mesh.indices.size());
}

void BoxApp::buildOctree() {
    std::vector<Octree::Entry> entries;
    entries.reserve(mSubmeshes.size());

    for (size_t submeshIndex = 0; submeshIndex < mSubmeshes.size(); ++submeshIndex) {
        entries.push_back({ submeshIndex, mSubmeshes[submeshIndex].bounds });
    }

    mSceneOctree.rebuild(entries, 24, 8);
}

std::vector<size_t> BoxApp::collectVisibleSubmeshes() const {
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    BoundingFrustum viewSpaceFrustum;
    BoundingFrustum::CreateFromMatrix(viewSpaceFrustum, proj);

    BoundingFrustum worldFrustum;
    const XMMATRIX invView = XMMatrixInverse(nullptr, view);
    viewSpaceFrustum.Transform(worldFrustum, invView);

    return mSceneOctree.query(worldFrustum);
}

void BoxApp::buildConstantBuffer()
{
    mObjectCB = new UploadBuffer<ObjectConstants>(md3dDevice.Get(), static_cast<UINT>(mSubmeshes.size()), true);
    mPassCB = new UploadBuffer<PassConstants>(md3dDevice.Get(), 1, true);
    mLightingCB = new UploadBuffer<LightingConstants>(md3dDevice.Get(), 1, true);
    mParticleSimCB = new UploadBuffer<ParticleSimConstants>(md3dDevice.Get(), 1, true);
}

void BoxApp::update(const GameTimer& gt) {
    if ((GetAsyncKeyState(VK_SPACE) & 0x0001) && mLights.size() < MAX_LIGHTS) {
        XMVECTOR anchorVec = XMLoadFloat3(&mEyePos);

        const float chainLength = 0.9f;

        XMFLOAT3 anchorPosition;
        XMStoreFloat3(&anchorPosition, anchorVec);

        LightData spotLight;
        spotLight.Type = static_cast<UINT>(LightType::Spot);
        spotLight.Position = XMFLOAT3(anchorPosition.x, anchorPosition.y - chainLength, anchorPosition.z);
        spotLight.Direction = XMFLOAT3(0.0f, -1.0f, 0.0f);
        spotLight.Color = XMFLOAT3(1.0f, 0.92f, 0.78f);
        spotLight.Intensity = 30.0f;
        spotLight.Range = 30.0f;
        spotLight.Attenuation = XMFLOAT3(1.0f, 0.045f, 0.0075f);
        spotLight.SpotAngle = XMConvertToRadians(38.0f);

        SwingingSpotLight swinging;
        swinging.Light = spotLight;
        swinging.AnchorPosition = anchorPosition;
        swinging.SpawnTime = gt.getTotalTime();
        swinging.PhaseOffset = gt.getTotalTime() * 0.73f + static_cast<float>(mSwingingSpotLights.size()) * 1.618f;

        mSwingingSpotLights.push_back(swinging);
        mLights.push_back(spotLight);
    }

    const float totalTime = gt.getTotalTime();
    for (size_t i = 0; i < mSwingingSpotLights.size(); ++i) {
        auto& swinging = mSwingingSpotLights[i];
        const float elapsed = totalTime - swinging.SpawnTime;

        const float swingAmplitude = XMConvertToRadians(40.0f);
        const float swingFrequency = 1.2f;
        const float chainLength = 0.9f;

        const float swingAngle = swingAmplitude * std::sin(elapsed * swingFrequency + swinging.PhaseOffset);

        XMVECTOR localPosition = XMVectorSet(0.0f, -chainLength, 0.0f, 0.0f);
        XMMATRIX rotation = XMMatrixRotationRollPitchYaw(swingAngle, 0.0f, 0.0f);
        XMVECTOR offset = XMVector3TransformNormal(localPosition, rotation);
        XMVECTOR anchorVec = XMLoadFloat3(&swinging.AnchorPosition);
        XMVECTOR position = XMVectorAdd(anchorVec, offset);

        XMFLOAT3 worldPosition;
        XMStoreFloat3(&worldPosition, position);
        swinging.Light.Position = worldPosition;

        swinging.Light.Direction = XMFLOAT3(0.0f, -1.0f, 0.0f);

        const size_t lightIndex = (mLights.size() - mSwingingSpotLights.size()) + i;
        if (lightIndex < mLights.size()) {
            mLights[lightIndex] = swinging.Light;
        }
    }

    if (GetAsyncKeyState('1') & 0x0001) {
        mEnableColumnVertexAnimation = !mEnableColumnVertexAnimation;
    }
    if (GetAsyncKeyState('2') & 0x0001) {
        mEnableColumnTextureAnimation = !mEnableColumnTextureAnimation;
    }
    if (GetAsyncKeyState('F') & 0x0001) {
        mEnableFrustumCulling = !mEnableFrustumCulling;
    }

    float dt = gt.getDeltaTime();
    float speed = SPEED_FACTOR * dt;

    if (GetAsyncKeyState('W') & 0x8000) {
        XMVECTOR s = XMVectorReplicate(speed);
        XMVECTOR l = XMLoadFloat3(&mLook);
        XMVECTOR p = XMLoadFloat3(&mEyePos);
        XMStoreFloat3(&mEyePos, XMVectorMultiplyAdd(s, l, p));
    }

    if (GetAsyncKeyState('S') & 0x8000) {
        XMVECTOR s = XMVectorReplicate(-speed);
        XMVECTOR l = XMLoadFloat3(&mLook);
        XMVECTOR p = XMLoadFloat3(&mEyePos);
        XMStoreFloat3(&mEyePos, XMVectorMultiplyAdd(s, l, p));
    }

    if (GetAsyncKeyState('A') & 0x8000) {
        XMVECTOR s = XMVectorReplicate(-speed);
        XMVECTOR r = XMLoadFloat3(&mRight);
        XMVECTOR p = XMLoadFloat3(&mEyePos);
        XMStoreFloat3(&mEyePos, XMVectorMultiplyAdd(s, r, p));
    }

    if (GetAsyncKeyState('D') & 0x8000) {
        XMVECTOR s = XMVectorReplicate(speed);
        XMVECTOR r = XMLoadFloat3(&mRight);
        XMVECTOR p = XMLoadFloat3(&mEyePos);
        XMStoreFloat3(&mEyePos, XMVectorMultiplyAdd(s, r, p));
    }

    XMVECTOR pos = XMLoadFloat3(&mEyePos);
    XMVECTOR target = pos + XMLoadFloat3(&mLook);
    XMVECTOR up = XMLoadFloat3(&mUp);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    if (mBillboardIndex != static_cast<size_t>(-1)) {
        XMVECTOR camPos = XMLoadFloat3(&mEyePos);
        XMVECTOR billPos = XMLoadFloat3(&mEarthBillboardPosition);
        XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        XMVECTOR forward = XMVectorSubtract(billPos, camPos);
        XMVECTOR target = XMVectorAdd(billPos, forward);

        XMMATRIX billboardView = XMMatrixLookAtLH(billPos, target, upVec);
        XMMATRIX billboardWorld = XMMatrixInverse(nullptr, billboardView);

        XMStoreFloat4x4(&mSubmeshWorlds[mBillboardIndex], billboardWorld);
    }

    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX texScale = XMMatrixScaling(TEXTURE_SCALE.x, TEXTURE_SCALE.y, TEXTURE_SCALE.z);
    for (size_t i = 0; i < mSubmeshes.size(); ++i) {
        const Submesh& submesh = mSubmeshes[i];
        const bool isColumn = isColumnSubmesh(submesh);
        const XMMATRIX world = XMLoadFloat4x4(&mSubmeshWorlds[i]);
        const XMMATRIX worldViewProj = world * view * proj;

        ObjectConstants objConstants = {};
        XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
        XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
        XMStoreFloat4x4(&objConstants.TextureTransform, XMMatrixTranspose(texScale));

        objConstants.TotalTime = isColumn ? gt.getTotalTime() : 0.0f;
        objConstants.VertexAnimationEnabled = isColumn && mEnableColumnVertexAnimation ? 1.0f : 0.0f;
        objConstants.TextureAnimationEnabled = isColumn && mEnableColumnTextureAnimation ? 1.0f : 0.0f;
        objConstants.DisplacementScale = DISPLACEMENT_SCALE;
        objConstants.MaxTessellationFactor = submesh.maxTessellationFactor;

        mObjectCB->copyData(static_cast<int>(i), objConstants);
    }

    XMMATRIX viewProj = view * proj;
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

    PassConstants passConstants = {};
    XMStoreFloat4x4(&passConstants.InvViewProj, XMMatrixTranspose(invViewProj));
    XMStoreFloat4x4(&passConstants.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&passConstants.Proj, XMMatrixTranspose(proj));
    passConstants.EyePosW = mEyePos;
    passConstants.AmbientColor = XMFLOAT4(0.08f, 0.08f, 0.1f, 1.0f);
    mPassCB->copyData(0, passConstants);

    LightingConstants lightingConstants = {};
    lightingConstants.LightCount = static_cast<UINT>(std::min(mLights.size(), static_cast<size_t>(MAX_LIGHTS)));
    for (UINT i = 0; i < lightingConstants.LightCount; ++i) {
        lightingConstants.Lights[i] = mLights[i];
    }
    mLightingCB->copyData(0, lightingConstants);

    ParticleSimConstants particleSim = {};
    particleSim.EmitterPosition = XMFLOAT3(0.f, 12.f, -15.f);
    particleSim.InitialVelocity = XMFLOAT3(0.f, 3.f, 0.f);
    particleSim.InitialSize = .35f;
    particleSim.InitialColor = XMFLOAT4(1.0f, 0.65f, 0.15f, 1.0f);
    particleSim.DeltaTime = gt.getDeltaTime();
    particleSim.TotalTime = gt.getTotalTime();
    particleSim.CameraPosition = mEyePos;
    particleSim.MinLifetime = 1.f;
    particleSim.MaxLifetime = 3.5f;
    particleSim.EmitCount = 96;
    mParticleSimCB->copyData(0, particleSim);
}

BoxApp::~BoxApp()
{
    delete mObjectCB;
    mObjectCB = nullptr;

    delete mPassCB;
    mPassCB = nullptr;

    delete mLightingCB;
    mLightingCB = nullptr;

    delete mParticleSimCB;
    mParticleSimCB = nullptr;
}

void BoxApp::onMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_LBUTTON) != 0) {
        float dx = XMConvertToRadians(0.15f * static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.15f * static_cast<float>(y - mLastMousePos.y));

        mYaw += dx;
        mPitch += dy;

        mPitch = std::clamp(mPitch, -XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f);
        
        XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(mPitch, mYaw, 0);

        XMVECTOR look = XMVectorSet(0, 0, 1, 0);
        look = XMVector3TransformNormal(look, rotationMatrix);
        XMStoreFloat3(&mLook, look);

        XMVECTOR right = XMVectorSet(1, 0, 0, 0);
        right = XMVector3TransformNormal(right, rotationMatrix);
        XMStoreFloat3(&mRight, right);
    }
    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void BoxApp::initializeConstants() {
    XMStoreFloat4x4(&mWorld, XMMatrixIdentity());
    XMStoreFloat4x4(&mView, XMMatrixIdentity());
    XMStoreFloat4x4(&mProj, XMMatrixIdentity());

    mLights.clear();
    mSwingingSpotLights.clear();

    LightData redPointLight;
    redPointLight.Type = static_cast<UINT>(LightType::Point);
    redPointLight.Position = XMFLOAT3(-1.6f, 2.8f, -1.2f);
    redPointLight.Color = XMFLOAT3(1.0f, 0.0f, 0.0f);
    redPointLight.Intensity = 15.0f;
    redPointLight.Range = 10.0f;
    redPointLight.Attenuation = XMFLOAT3(1.0f, 0.09f, 0.032f);
    mLights.push_back(redPointLight);

    LightData bluePointLight;
    bluePointLight.Type = static_cast<UINT>(LightType::Point);
    bluePointLight.Position = XMFLOAT3(1.6f, 2.8f, -1.2f);
    bluePointLight.Color = XMFLOAT3(0.0f, 0.0f, 1.0f);
    bluePointLight.Intensity = 15.0f;
    bluePointLight.Range = 10.0f;
    bluePointLight.Attenuation = XMFLOAT3(1.0f, 0.09f, 0.032f);
    mLights.push_back(bluePointLight);

    LightData directionalFill;
    directionalFill.Type = static_cast<UINT>(LightType::Directional);
    directionalFill.Direction = XMFLOAT3(-0.35f, -1.0f, -0.2f);
    directionalFill.Color = XMFLOAT3(0.8f, 0.85f, 1.0f);
    directionalFill.Intensity = 0.25f;
    mLights.push_back(directionalFill);
}

void BoxApp::draw(const GameTimer& gt)
{
    failCheck(mDirectCmdListAlloc->Reset());
    failCheck(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        getCurrentBackBufferResource(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    ID3D12DescriptorHeap* heaps[] = { mCbvSrvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    dispatchParticlePass(gt);

    mRenderingSystem->beginGeometryPass(mCommandList.Get(), getDepthStencilView());

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    mCommandList->IASetIndexBuffer(&mIndexBufferView);

    std::vector<size_t> visibleSubmeshIndices;
    if (mEnableFrustumCulling) {
        visibleSubmeshIndices = collectVisibleSubmeshes();
    }
    else {
        visibleSubmeshIndices.reserve(mSubmeshes.size());
        for (size_t submeshIndex = 0; submeshIndex < mSubmeshes.size(); ++submeshIndex) {
            visibleSubmeshIndices.push_back(submeshIndex);
        }
    }
    const float earthDistance = XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat3(&mEyePos), XMLoadFloat3(&mEarthPosition))));
    const bool drawEarthMesh = earthDistance <= EARTH_BILLBOARD_SWITCH_DISTANCE;

    for (size_t submeshIndex : visibleSubmeshIndices) {
        const auto& submesh = mSubmeshes[submeshIndex];
        const bool isBillboard = (submeshIndex == mBillboardIndex);
        const bool isEarthSubmesh = std::binary_search(mEarthSubmeshIndices.begin(), mEarthSubmeshIndices.end(), submeshIndex);

        if (isBillboard && drawEarthMesh) {
            continue;
        }
        if (isEarthSubmesh && !drawEarthMesh) {
            continue;
        }

        const bool isColumn = isColumnSubmesh(submesh);
        const bool useTessellation = !isColumn && hasDisplacementTexture(submesh);

        if (useTessellation) {
            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
            mCommandList->SetPipelineState(mEarthTessPSO.Get());
        }
        else {
            mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCommandList->SetPipelineState(isColumn ? mColumnPSO.Get() : mPSO.Get());
        }


        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
        cbvHandle.Offset(submesh.objectCbvHeapIndex, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE passCbvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getPassCbvIndex(), mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
        srvHandle.Offset(submesh.material.diffuseSrvHeapIndex, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(2, srvHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE normalSrvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
        normalSrvHandle.Offset(submesh.material.normalSrvHeapIndex, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(3, normalSrvHandle);

        CD3DX12_GPU_DESCRIPTOR_HANDLE displacementSrvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
        displacementSrvHandle.Offset(submesh.material.displacementSrvHeapIndex, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(4, displacementSrvHandle);

        mCommandList->DrawIndexedInstanced(submesh.indexCount, 1, submesh.startIndiceIndex, 0, 0);
    }

    mRenderingSystem->endGeometryPass(mCommandList.Get());

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(getCurrentBackBuffer());
    const float clearColor[4] = { 0.2f, 0.2f, 0.3f, 1.0f };
    mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    mRenderingSystem->beginLightingPass(mCommandList.Get(), rtvHandle);
    mCommandList->SetGraphicsRootSignature(mLightingRootSignature.Get());
    mCommandList->SetPipelineState(mLightingPSO.Get());

    CD3DX12_GPU_DESCRIPTOR_HANDLE passCbvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getPassCbvIndex(), mCbvSrvDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(0, passCbvHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE gbufferSrvHandle = mRenderingSystem->getGBuffer()->getSrvHandle();
    mCommandList->SetGraphicsRootDescriptorTable(1, gbufferSrvHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE lightingCbvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getLightingCbvIndex(), mCbvSrvDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(2, lightingCbvHandle);

    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->DrawInstanced(3, 1, 0, 0);

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = getDepthStencilView();
    mCommandList->OMSetRenderTargets(1, &rtvHandle, TRUE, &dsvHandle);
    mCommandList->SetGraphicsRootSignature(mParticleRootSignature.Get());
    mCommandList->SetPipelineState(mParticlePSO.Get());
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
    mCommandList->IASetVertexBuffers(0, 1, &mParticleIndexBufferView);

    CD3DX12_GPU_DESCRIPTOR_HANDLE particlePassCbvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getPassCbvIndex(), mCbvSrvDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(0, particlePassCbvHandle);
    CD3DX12_GPU_DESCRIPTOR_HANDLE particleSrvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getParticlePoolSrvIndex(), mCbvSrvDescriptorSize);
    mCommandList->SetGraphicsRootDescriptorTable(1, particleSrvHandle);
    mCommandList->DrawInstanced(PARTICLE_COUNT, 1, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        getCurrentBackBufferResource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier);

    failCheck(mCommandList->Close());
    ID3D12CommandList* cmds[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(1, cmds);

    failCheck(mSwapChain->Present(1, 0));

    mCurrBackBuffer = (mCurrBackBuffer + 1) % swapChainBufferCount;

    flushCommandQueue();
}

void BoxApp::buildRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE cbvRange;
    cbvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE passCbvRange;
    passCbvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE normalSrvRange;
    normalSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

    CD3DX12_DESCRIPTOR_RANGE displacementSrvRange;
    displacementSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

    CD3DX12_ROOT_PARAMETER slotRootParameter[5];
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvRange, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[1].InitAsDescriptorTable(1, &passCbvRange, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[2].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[3].InitAsDescriptorTable(1, &normalSrvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[4].InitAsDescriptorTable(1, &displacementSrvRange, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_STATIC_SAMPLER_DESC staticSampler(0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, 1, &staticSampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;
    D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(),
        errorBlob.GetAddressOf());

    failCheck(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::buildLightingRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE cbvRange;
    cbvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE gbufferSrvRange;
    gbufferSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, GBuffer::mTexturesNum, 0);

    CD3DX12_DESCRIPTOR_RANGE lightingCbvRange;
    lightingCbvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

    CD3DX12_ROOT_PARAMETER slotRootParameter[3];
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsDescriptorTable(1, &gbufferSrvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[2].InitAsDescriptorTable(1, &lightingCbvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC staticSampler(0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 1, &staticSampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;
    D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(),
        errorBlob.GetAddressOf());

    failCheck(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&mLightingRootSignature)));
}

void BoxApp::buildParticleRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE passCbvRange;
    passCbvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE particleSrvRange;
    particleSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsDescriptorTable(1, &passCbvRange, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[1].InitAsDescriptorTable(1, &particleSrvRange, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;
    D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(),
        errorBlob.GetAddressOf());

    failCheck(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mParticleRootSignature)));
}

void BoxApp::buildParticleComputeRootSignature() {
    CD3DX12_DESCRIPTOR_RANGE particleUavRange;
    particleUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE deadListUavRange;
    deadListUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

    CD3DX12_DESCRIPTOR_RANGE deadListConsumeUavRange;
    deadListConsumeUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

    CD3DX12_DESCRIPTOR_RANGE sortListUavRange;
    sortListUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);

    CD3DX12_DESCRIPTOR_RANGE simCbvRange;
    simCbvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[5];
    slotRootParameter[0].InitAsDescriptorTable(1, &particleUavRange, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[1].InitAsDescriptorTable(1, &deadListUavRange, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[2].InitAsDescriptorTable(1, &deadListConsumeUavRange, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[3].InitAsDescriptorTable(1, &sortListUavRange, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[4].InitAsDescriptorTable(1, &simCbvRange, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;
    D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    failCheck(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mParticleComputeRootSignature)));
}

void BoxApp::buildPso(const std::wstring& shaderName, ComPtr<ID3D12PipelineState>& pso, bool enableTessellation) {
    ComPtr<ID3DBlob> mvsByteCode;
    ComPtr<ID3DBlob> mpsByteCode;
    ComPtr<ID3DBlob> mhsByteCode;
    ComPtr<ID3DBlob> mdsByteCode;

    mvsByteCode = D3DUtil::compileShader(shaderName, nullptr, enableTessellation ? "VS_Tess" : "VS", "vs_5_0");
    mpsByteCode = D3DUtil::compileShader(shaderName, nullptr, "PS", "ps_5_0");
    if (enableTessellation) {
        mhsByteCode = D3DUtil::compileShader(shaderName, nullptr, "HS", "hs_5_0");
        mdsByteCode = D3DUtil::compileShader(shaderName, nullptr, "DS", "ds_5_0");
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    const bool isLightingPass = (shaderName == L"lighting_shader.hlsl");
    psoDesc.pRootSignature = isLightingPass ? mLightingRootSignature.Get() : mRootSignature.Get();

    psoDesc.VS = {
        reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
        mvsByteCode->GetBufferSize()
    };

    psoDesc.PS = {
        reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };

    if (enableTessellation) {
        psoDesc.HS = {
            reinterpret_cast<BYTE*>(mhsByteCode->GetBufferPointer()),
            mhsByteCode->GetBufferSize()
        };
        psoDesc.DS = {
            reinterpret_cast<BYTE*>(mdsByteCode->GetBufferPointer()),
            mdsByteCode->GetBufferSize()
        };
    }

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = enableTessellation ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    if (isLightingPass) {
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = mBackBufferFormat;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.InputLayout = { nullptr, 0 };
    }
    else {
        psoDesc.NumRenderTargets = GBuffer::mTexturesNum;
        psoDesc.RTVFormats[0] = mRenderingSystem->getGBuffer()->getFormat(0);
        psoDesc.RTVFormats[1] = mRenderingSystem->getGBuffer()->getFormat(1);
        psoDesc.RTVFormats[2] = mRenderingSystem->getGBuffer()->getFormat(2);
        psoDesc.DSVFormat = mDepthStencilFormat;
        psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    }
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    failCheck(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

void BoxApp::buildParticlePso() {
    ComPtr<ID3DBlob> vsByteCode = D3DUtil::compileShader(L"particle_shader.hlsl", nullptr, "VS", "vs_5_0");
    ComPtr<ID3DBlob> gsByteCode = D3DUtil::compileShader(L"particle_shader.hlsl", nullptr, "GS", "gs_5_0");
    ComPtr<ID3DBlob> psByteCode = D3DUtil::compileShader(L"particle_shader.hlsl", nullptr, "PS", "ps_5_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = mParticleRootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(vsByteCode->GetBufferPointer()), vsByteCode->GetBufferSize() };
    psoDesc.GS = { reinterpret_cast<BYTE*>(gsByteCode->GetBufferPointer()), gsByteCode->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(psByteCode->GetBufferPointer()), psByteCode->GetBufferSize() };
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.DSVFormat = mDepthStencilFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "PARTICLEID", 0, DXGI_FORMAT_R32_UINT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    psoDesc.InputLayout = { layout, _countof(layout) };
    failCheck(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mParticlePSO)));

    ComPtr<ID3DBlob> emitCs = D3DUtil::compileShader(L"particle_compute.hlsl", nullptr, "EmitCS", "cs_5_0");
    ComPtr<ID3DBlob> simulateCs = D3DUtil::compileShader(L"particle_compute.hlsl", nullptr, "SimulateCS", "cs_5_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc = {};
    computeDesc.pRootSignature = mParticleComputeRootSignature.Get();
    computeDesc.CS = { reinterpret_cast<BYTE*>(emitCs->GetBufferPointer()), emitCs->GetBufferSize() };
    failCheck(md3dDevice->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&mParticleEmitPSO)));

    computeDesc.CS = { reinterpret_cast<BYTE*>(simulateCs->GetBufferPointer()), simulateCs->GetBufferSize() };
    failCheck(md3dDevice->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&mParticleSimulatePSO)));
}

void BoxApp::buildParticleResources() {
    const UINT64 particlePoolByteSize = sizeof(ParticleData) * PARTICLE_COUNT;
    const UINT64 deadListByteSize = sizeof(UINT) * PARTICLE_COUNT;
    const UINT64 sortListByteSize = sizeof(XMFLOAT2) * PARTICLE_COUNT;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);

    auto createUpload = [this, &uploadHeap](UINT64 size, ComPtr<ID3D12Resource>& uploadResource) {
        D3D12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        failCheck(md3dDevice->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadResource)));
        };

    D3D12_RESOURCE_DESC particlePoolDesc = CD3DX12_RESOURCE_DESC::Buffer(particlePoolByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    failCheck(md3dDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &particlePoolDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mParticlePoolBuffer)));
    createUpload(particlePoolByteSize, mParticlePoolUpload);
    std::vector<ParticleData> initialParticles(PARTICLE_COUNT);
    void* mappedPtr = nullptr;
    failCheck(mParticlePoolUpload->Map(0, nullptr, &mappedPtr));
    std::memcpy(mappedPtr, initialParticles.data(), static_cast<size_t>(particlePoolByteSize));
    mParticlePoolUpload->Unmap(0, nullptr);
    mCommandList->CopyBufferRegion(mParticlePoolBuffer.Get(), 0, mParticlePoolUpload.Get(), 0, particlePoolByteSize);

    D3D12_RESOURCE_DESC deadListDesc = CD3DX12_RESOURCE_DESC::Buffer(deadListByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    failCheck(md3dDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &deadListDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mDeadListBuffer)));
    createUpload(deadListByteSize, mDeadListUpload);
    std::vector<UINT> deadListData(PARTICLE_COUNT);
    for (UINT i = 0; i < PARTICLE_COUNT; ++i) deadListData[i] = i;
    failCheck(mDeadListUpload->Map(0, nullptr, &mappedPtr));
    std::memcpy(mappedPtr, deadListData.data(), static_cast<size_t>(deadListByteSize));
    mDeadListUpload->Unmap(0, nullptr);
    mCommandList->CopyBufferRegion(mDeadListBuffer.Get(), 0, mDeadListUpload.Get(), 0, deadListByteSize);

    D3D12_RESOURCE_DESC counterDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    failCheck(md3dDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &counterDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mDeadListCounterBuffer)));
    createUpload(sizeof(UINT), mDeadListCounterUpload);
    const UINT initialCounter = PARTICLE_COUNT;
    failCheck(mDeadListCounterUpload->Map(0, nullptr, &mappedPtr));
    std::memcpy(mappedPtr, &initialCounter, sizeof(UINT));
    mDeadListCounterUpload->Unmap(0, nullptr);
    mCommandList->CopyBufferRegion(mDeadListCounterBuffer.Get(), 0, mDeadListCounterUpload.Get(), 0, sizeof(UINT));

    D3D12_RESOURCE_DESC sortListDesc = CD3DX12_RESOURCE_DESC::Buffer(sortListByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    failCheck(md3dDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &sortListDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mSortListBuffer)));

    const UINT64 indexByteSize = sizeof(UINT) * PARTICLE_COUNT;
    D3D12_RESOURCE_DESC indexDesc = CD3DX12_RESOURCE_DESC::Buffer(indexByteSize);
    failCheck(md3dDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &indexDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mParticleIndexBuffer)));
    createUpload(indexByteSize, mParticleIndexUpload);
    std::vector<UINT> indices(PARTICLE_COUNT);
    for (UINT i = 0; i < PARTICLE_COUNT; ++i) indices[i] = i;
    failCheck(mParticleIndexUpload->Map(0, nullptr, &mappedPtr));
    std::memcpy(mappedPtr, indices.data(), static_cast<size_t>(indexByteSize));
    mParticleIndexUpload->Unmap(0, nullptr);
    mCommandList->CopyBufferRegion(mParticleIndexBuffer.Get(), 0, mParticleIndexUpload.Get(), 0, indexByteSize);

    mParticleIndexBufferView.BufferLocation = mParticleIndexBuffer->GetGPUVirtualAddress();
    mParticleIndexBufferView.StrideInBytes = sizeof(UINT);
    mParticleIndexBufferView.SizeInBytes = static_cast<UINT>(indexByteSize);

    CD3DX12_RESOURCE_BARRIER initBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(mParticlePoolBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(mDeadListBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(mDeadListCounterBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        CD3DX12_RESOURCE_BARRIER::Transition(mParticleIndexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
    };
    mCommandList->ResourceBarrier(_countof(initBarriers), initBarriers);
}

void BoxApp::onResize() {
    D3DApp::onResize();

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, getAspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);

    if (mRenderingSystem) {
        mRenderingSystem->onResize(mClientWidth, mClientHeight);
    }
}

void BoxApp::dispatchParticlePass(const GameTimer&) {
    CD3DX12_RESOURCE_BARRIER toCompute = CD3DX12_RESOURCE_BARRIER::Transition(
        mParticlePoolBuffer.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    mCommandList->ResourceBarrier(1, &toCompute);

    mCommandList->SetComputeRootSignature(mParticleComputeRootSignature.Get());
    mCommandList->SetPipelineState(mParticleEmitPSO.Get());
    CD3DX12_GPU_DESCRIPTOR_HANDLE particlePoolUavHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getParticlePoolUavIndex(), mCbvSrvDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE deadListUavHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getDeadListUavIndex(), mCbvSrvDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE deadListConsumeUavHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getDeadListConsumeUavIndex(), mCbvSrvDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE sortListUavHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getSortListUavIndex(), mCbvSrvDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE simCbvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getLightingCbvIndex() + 1, mCbvSrvDescriptorSize);
    mCommandList->SetComputeRootDescriptorTable(0, particlePoolUavHandle);
    mCommandList->SetComputeRootDescriptorTable(1, deadListUavHandle);
    mCommandList->SetComputeRootDescriptorTable(2, deadListConsumeUavHandle);
    mCommandList->SetComputeRootDescriptorTable(3, sortListUavHandle);
    mCommandList->SetComputeRootDescriptorTable(4, simCbvHandle);
    mCommandList->Dispatch(1, 1, 1);

    mCommandList->SetPipelineState(mParticleSimulatePSO.Get());
    mCommandList->Dispatch((PARTICLE_COUNT + PARTICLE_CS_GROUP_SIZE - 1) / PARTICLE_CS_GROUP_SIZE, 1, 1);

    CD3DX12_RESOURCE_BARRIER toRender = CD3DX12_RESOURCE_BARRIER::Transition(
        mParticlePoolBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &toRender);
}

void BoxApp::loadTextures() {
    const auto loadTextureDirectory = [this](const std::wstring& textureDir) {
        for (auto& entry : std::filesystem::directory_iterator(textureDir)) {
            if (!entry.is_regular_file())
                continue;

            auto path = entry.path();
            if (path.extension() != L".dds")
                continue;

            std::unique_ptr<Texture> texture = std::make_unique<Texture>();
            texture->fileName = path.stem().wstring();
            texture->filePath = path.wstring();

            failCheck(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
                texture->filePath.c_str(), texture->resource, texture->uploadHeap));

            mTextures[texture->fileName] = std::move(texture);
        }
        };

    loadTextureDirectory(L"sponza/");
    loadTextureDirectory(L"earth/");
}

void BoxApp::buildCbvSrvHeap() {
    UINT numTextures = static_cast<UINT>(mTextures.size());
    const UINT objectCbvCount = static_cast<UINT>(mSubmeshes.size());
    UINT numDescriptors = objectCbvCount + 3 + GBuffer::mTexturesNum + 3 + numTextures + 5;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask = 0;
    failCheck(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mCbvSrvHeap)));

    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = mObjectCB->getResource()->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = D3DUtil::calcConstantBufferByteSize(sizeof(ObjectConstants));
    for (size_t i = 0; i < mSubmeshes.size(); ++i) {
        auto& submesh = mSubmeshes[i];
        md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
        submesh.objectCbvHeapIndex = static_cast<UINT>(i);
        handle.Offset(1, mCbvSrvDescriptorSize);
        cbvDesc.BufferLocation += cbvDesc.SizeInBytes;
    }

    cbvDesc.BufferLocation = mPassCB->getResource()->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = D3DUtil::calcConstantBufferByteSize(sizeof(PassConstants));
    md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    handle.Offset(1, mCbvSrvDescriptorSize);

    cbvDesc.BufferLocation = mLightingCB->getResource()->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = D3DUtil::calcConstantBufferByteSize(sizeof(LightingConstants));
    md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    handle.Offset(1, mCbvSrvDescriptorSize);

    cbvDesc.BufferLocation = mParticleSimCB->getResource()->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = D3DUtil::calcConstantBufferByteSize(sizeof(ParticleSimConstants));
    md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    handle.Offset(1, mCbvSrvDescriptorSize);

    mRenderingSystem = std::make_unique<RenderingSystem>(md3dDevice.Get(), mClientWidth, mClientHeight);

    CD3DX12_CPU_DESCRIPTOR_HANDLE gbufferSrvStart(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), getGBufferSrvStartIndex(), mCbvSrvDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gbufferGpuSrvStart(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart(), getGBufferSrvStartIndex(), mCbvSrvDescriptorSize);

    CD3DX12_CPU_DESCRIPTOR_HANDLE gbufferRtvStart(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), swapChainBufferCount, mRtvDescriptorSize);

    mRenderingSystem->buildDescriptors(gbufferSrvStart, gbufferGpuSrvStart, gbufferRtvStart, mCbvSrvDescriptorSize, mRtvDescriptorSize);

    handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), getDefaultTextureSrvStartIndex(), mCbvSrvDescriptorSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    md3dDevice->CreateShaderResourceView(mDefaultDiffuseTex.Get(), &srvDesc, handle);

    handle.Offset(1, mCbvSrvDescriptorSize);
    md3dDevice->CreateShaderResourceView(mDefaultNormalTex.Get(), &srvDesc, handle);

    handle.Offset(1, mCbvSrvDescriptorSize);
    md3dDevice->CreateShaderResourceView(mDefaultDisplacementTex.Get(), &srvDesc, handle);

    UINT i = getDefaultTextureSrvStartIndex() + 3;
    for (auto& kv : mTextures) {
        Texture* texture = kv.second.get();
        srvDesc.Format = texture->resource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = texture->resource->GetDesc().MipLevels;

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), i, mCbvSrvDescriptorSize);
        md3dDevice->CreateShaderResourceView(texture->resource.Get(), &srvDesc, srvHandle);

        texture->srvHeapIndex = i;
        ++i;
    }
}

void BoxApp::bindMaterialsToTextures() {
    for (auto& submesh : mSubmeshes) {
        const std::string& texName = submesh.material.diffuseTextureName;
        const std::string& normalTexName = submesh.material.normalTextureName;
        const std::string& displacementTexName = submesh.material.displacementTextureName;
        const UINT defaultDiffuseSrvIndex = getDefaultTextureSrvStartIndex();
        const UINT defaultNormalSrvIndex = getDefaultTextureSrvStartIndex() + 1;
        const UINT defaultDisplacementSrvIndex = getDefaultTextureSrvStartIndex() + 2;
        if (texName.empty()) {
            submesh.material.diffuseSrvHeapIndex = defaultDiffuseSrvIndex;
        }
        else {
            auto it = mTextures.find(std::wstring(texName.begin(), texName.end()));
            submesh.material.diffuseSrvHeapIndex = (it != mTextures.end()) ? it->second->srvHeapIndex : defaultDiffuseSrvIndex;
        }

        if (normalTexName.empty()) {
            submesh.material.normalSrvHeapIndex = defaultNormalSrvIndex;
        }
        else {
            auto it = mTextures.find(std::wstring(normalTexName.begin(), normalTexName.end()));
            submesh.material.normalSrvHeapIndex = (it != mTextures.end()) ? it->second->srvHeapIndex : defaultNormalSrvIndex;
        }

        if (displacementTexName.empty()) {
            submesh.material.displacementSrvHeapIndex = defaultDisplacementSrvIndex;
        }
        else {
            auto it = mTextures.find(std::wstring(displacementTexName.begin(), displacementTexName.end()));
            submesh.material.displacementSrvHeapIndex = (it != mTextures.end()) ? it->second->srvHeapIndex : defaultDisplacementSrvIndex;
        }
    }
}

void BoxApp::buildParticleDescriptors() {
    D3D12_SHADER_RESOURCE_VIEW_DESC particleSrvDesc = {};
    particleSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    particleSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    particleSrvDesc.Buffer.FirstElement = 0;
    particleSrvDesc.Buffer.NumElements = PARTICLE_COUNT;
    particleSrvDesc.Buffer.StructureByteStride = sizeof(ParticleData);
    particleSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    particleSrvDesc.Format = DXGI_FORMAT_UNKNOWN;

    CD3DX12_CPU_DESCRIPTOR_HANDLE particleSrvHandle(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), getParticlePoolSrvIndex(), mCbvSrvDescriptorSize);
    md3dDevice->CreateShaderResourceView(mParticlePoolBuffer.Get(), &particleSrvDesc, particleSrvHandle);

    D3D12_UNORDERED_ACCESS_VIEW_DESC particleUavDesc = {};
    particleUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    particleUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    particleUavDesc.Buffer.FirstElement = 0;
    particleUavDesc.Buffer.NumElements = PARTICLE_COUNT;
    particleUavDesc.Buffer.StructureByteStride = sizeof(ParticleData);
    particleUavDesc.Buffer.CounterOffsetInBytes = 0;
    particleUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    CD3DX12_CPU_DESCRIPTOR_HANDLE particleUavHandle(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), getParticlePoolUavIndex(), mCbvSrvDescriptorSize);
    md3dDevice->CreateUnorderedAccessView(mParticlePoolBuffer.Get(), nullptr, &particleUavDesc, particleUavHandle);

    D3D12_UNORDERED_ACCESS_VIEW_DESC deadUavDesc = {};
    deadUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    deadUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    deadUavDesc.Buffer.FirstElement = 0;
    deadUavDesc.Buffer.NumElements = PARTICLE_COUNT;
    deadUavDesc.Buffer.StructureByteStride = sizeof(UINT);
    deadUavDesc.Buffer.CounterOffsetInBytes = 0;
    deadUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    CD3DX12_CPU_DESCRIPTOR_HANDLE deadUavHandle(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), getDeadListUavIndex(), mCbvSrvDescriptorSize);
    md3dDevice->CreateUnorderedAccessView(mDeadListBuffer.Get(), mDeadListCounterBuffer.Get(), &deadUavDesc, deadUavHandle);

    D3D12_UNORDERED_ACCESS_VIEW_DESC deadConsumeUavDesc = {};
    deadConsumeUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    deadConsumeUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    deadConsumeUavDesc.Buffer.FirstElement = 0;
    deadConsumeUavDesc.Buffer.NumElements = PARTICLE_COUNT;
    deadConsumeUavDesc.Buffer.StructureByteStride = sizeof(UINT);
    deadConsumeUavDesc.Buffer.CounterOffsetInBytes = 0;
    deadConsumeUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    CD3DX12_CPU_DESCRIPTOR_HANDLE deadConsumeUavHandle(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), getDeadListConsumeUavIndex(), mCbvSrvDescriptorSize);
    md3dDevice->CreateUnorderedAccessView(mDeadListBuffer.Get(), mDeadListCounterBuffer.Get(), &deadConsumeUavDesc, deadConsumeUavHandle);

    D3D12_UNORDERED_ACCESS_VIEW_DESC sortUavDesc = {};
    sortUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    sortUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortUavDesc.Buffer.FirstElement = 0;
    sortUavDesc.Buffer.NumElements = PARTICLE_COUNT;
    sortUavDesc.Buffer.StructureByteStride = sizeof(XMFLOAT2);
    sortUavDesc.Buffer.CounterOffsetInBytes = 0;
    sortUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    CD3DX12_CPU_DESCRIPTOR_HANDLE sortUavHandle(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart(), getSortListUavIndex(), mCbvSrvDescriptorSize);
    md3dDevice->CreateUnorderedAccessView(mSortListBuffer.Get(), nullptr, &sortUavDesc, sortUavHandle);
}

UINT BoxApp::getPassCbvIndex() const {
    return static_cast<UINT>(mSubmeshes.size());
}

UINT BoxApp::getLightingCbvIndex() const {
    return getPassCbvIndex() + 1;
}

UINT BoxApp::getGBufferSrvStartIndex() const {
    return getLightingCbvIndex() + 2;
}

UINT BoxApp::getDefaultTextureSrvStartIndex() const {
    return getGBufferSrvStartIndex() + GBuffer::mTexturesNum;
}

UINT BoxApp::getParticlePoolSrvIndex() const {
    return getDefaultTextureSrvStartIndex() + 3 + static_cast<UINT>(mTextures.size());
}

UINT BoxApp::getParticlePoolUavIndex() const {
    return getParticlePoolSrvIndex() + 1;
}

UINT BoxApp::getDeadListUavIndex() const {
    return getParticlePoolUavIndex() + 1;
}

UINT BoxApp::getDeadListConsumeUavIndex() const {
    return getDeadListUavIndex() + 1;
}

UINT BoxApp::getSortListUavIndex() const {
    return getDeadListConsumeUavIndex() + 1;
}

void BoxApp::createDefaultTextures() {
    const auto createSolidTexture = [this](uint32_t color, ComPtr<ID3D12Resource>& textureResource, ComPtr<ID3D12Resource>& uploadResource) {
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Alignment = 0;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        failCheck(md3dDevice->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&textureResource)));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(textureResource.Get(), 0, 1);
        CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        failCheck(md3dDevice->CreateCommittedResource(
            &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadResource)));

        D3D12_SUBRESOURCE_DATA texData = {};
        texData.pData = &color;
        texData.RowPitch = 4;
        texData.SlicePitch = texData.RowPitch;

        UpdateSubresources(mCommandList.Get(), textureResource.Get(), uploadResource.Get(), 0, 0, 1, &texData);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            textureResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        mCommandList->ResourceBarrier(1, &barrier);
        };

    createSolidTexture(0xffffffff, mDefaultDiffuseTex, mDefaultDiffuseTexUpload);
    createSolidTexture(0xffff8080, mDefaultNormalTex, mDefaultNormalTexUpload);
    createSolidTexture(0xff000000, mDefaultDisplacementTex, mDefaultDisplacementTexUpload);
}