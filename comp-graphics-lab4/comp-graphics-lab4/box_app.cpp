#include "box_app.h"
#include "fail_checker.h"
#include "model_loader.h"
#include "DDSTextureLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <DirectXColors.h>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

void BoxApp::buildResources() {
    initializeConstants();

    failCheck(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
    buildBuffers();
    loadTextures();
    buildRootSignature();
    buildConstantBuffer();
    buildCbvSrvHeap();
    bindMaterialsToTextures();
    buildPso(L"main_shader.hlsl", mPSO);
    buildPso(L"column_shader.hlsl", mPSOColumn);
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
    ModelLoader loader(SCENE_SCALE);
    MeshData mesh = loader.loadModel("sponza.obj");

    const UINT vbByteSize = static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex));
    const UINT ibByteSize = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));

    mVertexBufferGPU = D3DUtil::createDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
        mesh.vertices.data(), vbByteSize, mVertexBufferUploader);

    mIndexBufferGPU = D3DUtil::createDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
        mesh.indices.data(), ibByteSize, mIndexBufferUploader);

    UINT indexOffset = 0;
    UINT vertexOffset = 0;
    for (const auto& sm : mesh.submeshes) {
        Submesh submesh(sm);
        mSubmeshes.push_back(submesh);
    }

    mInputLayout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    mVertexBufferView.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
    mVertexBufferView.StrideInBytes = sizeof(Vertex);
    mVertexBufferView.SizeInBytes = vbByteSize;

    mIndexBufferView.BufferLocation = mIndexBufferGPU->GetGPUVirtualAddress();
    mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    mIndexBufferView.SizeInBytes = ibByteSize;

    mIndexCount = static_cast<UINT>(mesh.indices.size());
}

void BoxApp::buildConstantBuffer()
{
    mObjectCB = new UploadBuffer<ObjectConstants>(md3dDevice.Get(), 2, true);
}

void BoxApp::update(const GameTimer& gt)
{
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

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX worldViewProj = world * view * proj;

    mTextureOffset.x += mTextureScrollSpeedX;
    mTextureOffset.y += mTextureScrollSpeedY;
    if (mTextureOffset.x > 1.f) mTextureOffset.x -= 1.f;
    if (mTextureOffset.y > 1.f) mTextureOffset.y -= 1.f;

    ObjectConstants objConstantsNoAnim;
    XMStoreFloat4x4(&objConstantsNoAnim.WorldViewProj, XMMatrixTranspose(worldViewProj));
    XMStoreFloat4x4(&objConstantsNoAnim.World, XMMatrixTranspose(world));

    XMMATRIX texScale = XMMatrixScaling(TEXTURE_SCALE.x, TEXTURE_SCALE.y, TEXTURE_SCALE.z);
    XMStoreFloat4x4(&objConstantsNoAnim.TextureTransform, XMMatrixTranspose(texScale));

    mObjectCB->copyData(0, objConstantsNoAnim);

    ObjectConstants objConstantsAnim;
    XMStoreFloat4x4(&objConstantsAnim.WorldViewProj, XMMatrixTranspose(worldViewProj));
    XMStoreFloat4x4(&objConstantsAnim.World, XMMatrixTranspose(world));

    XMMATRIX texScaleAnim = XMMatrixScaling(TEXTURE_SCALE.x, TEXTURE_SCALE.y, TEXTURE_SCALE.z);
    XMMATRIX texOffset = XMMatrixTranslation(mTextureOffset.x, mTextureOffset.y, 0.f);
    XMMATRIX texTransformAnim = texScaleAnim * texOffset;
    XMStoreFloat4x4(&objConstantsAnim.TextureTransform, XMMatrixTranspose(texTransformAnim));

    mObjectCB->copyData(1, objConstantsAnim);
}

BoxApp::~BoxApp()
{
    delete mObjectCB;
    mObjectCB = nullptr;
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
}

void BoxApp::draw(const GameTimer& gt)
{
    failCheck(mDirectCmdListAlloc->Reset());
    failCheck(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        getCurrentBackBufferResource(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(getCurrentBackBuffer());

    const float clearColor[4] = { 0.2f, 0.2f, 0.3f, 1.0f };
    mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    mCommandList->ClearDepthStencilView(getDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    auto depthStencilView = getDepthStencilView();
    mCommandList->OMSetRenderTargets(1, &rtvHandle, TRUE, &depthStencilView);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = { mCbvSrvHeap.Get()};
    mCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
    mCommandList->SetGraphicsRootDescriptorTable(0, cbvHandle);

    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    mCommandList->IASetIndexBuffer(&mIndexBufferView);

    for (const auto& submesh : mSubmeshes) {
        if (submesh.material.diffuseTextureName.find("column") != std::string::npos) {
            mCommandList->SetPipelineState(mPSOColumn.Get());
        }
        else {
            mCommandList->SetPipelineState(mPSO.Get());
        }

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
        srvHandle.Offset(submesh.material.diffuseSrvHeapIndex, mCbvSrvDescriptorSize);
        mCommandList->SetGraphicsRootDescriptorTable(1, srvHandle);

        mCommandList->DrawIndexedInstanced(submesh.indexCount, 1, submesh.startIndiceIndex, 0, 0);
    }

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

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvRange, D3D12_SHADER_VISIBILITY_ALL);
    slotRootParameter[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC staticSampler(0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 1, &staticSampler,
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

void BoxApp::buildPso(const std::wstring& shaderName, ComPtr<ID3D12PipelineState>& pso) {
    ComPtr<ID3DBlob> mvsByteCode;
    ComPtr<ID3DBlob> mpsByteCode;

    mvsByteCode = D3DUtil::compileShader(shaderName, nullptr, "VS", "vs_5_0");
    mpsByteCode = D3DUtil::compileShader(shaderName, nullptr, "PS", "ps_5_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.pRootSignature = mRootSignature.Get();

    psoDesc.VS = {
        reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
        mvsByteCode->GetBufferSize()
    };

    psoDesc.PS = {
        reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.DSVFormat = mDepthStencilFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    failCheck(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
}

void BoxApp::onResize() {
    D3DApp::onResize();

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, getAspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void BoxApp::loadTextures() {
    const std::wstring textureDir = L"textures/";
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
}

void BoxApp::buildCbvSrvHeap() {
    UINT numTextures = static_cast<UINT>(mTextures.size());
    UINT numDescriptors = 3 + numTextures;

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
    md3dDevice->CreateConstantBufferView(&cbvDesc, handle);

    handle.Offset(1, mCbvSrvDescriptorSize);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    md3dDevice->CreateShaderResourceView(mDefaultTex.Get(), &srvDesc, handle);

    UINT i = 2;
    for (auto& kv : mTextures) {
        Texture* texture = kv.second.get();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texture->resource->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
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

        if (texName.empty()) {
            submesh.material.diffuseSrvHeapIndex = 1;
            continue;
        }

        auto it = mTextures.find(std::wstring(texName.begin(), texName.end()));

        if (it != mTextures.end()) {
            submesh.material.diffuseSrvHeapIndex = it->second->srvHeapIndex;
        }
        else {
            submesh.material.diffuseSrvHeapIndex = 1;
        }
    }
}

void BoxApp::createDefaultTexture() {
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
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mDefaultTex)));

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mDefaultTex.Get(), 0, 1);
    ComPtr<ID3D12Resource> textureUploadHeap;
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    failCheck(md3dDevice->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&textureUploadHeap)));

    uint32_t whiteColor = 0xffffffff;
    D3D12_SUBRESOURCE_DATA texData = {};
    texData.pData = &whiteColor;
    texData.RowPitch = 4;
    texData.SlicePitch = texData.RowPitch;

    UpdateSubresources(mCommandList.Get(), mDefaultTex.Get(), textureUploadHeap.Get(), 0, 0, 1, &texData);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mDefaultTex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    mCommandList->ResourceBarrier(1, &barrier);
}