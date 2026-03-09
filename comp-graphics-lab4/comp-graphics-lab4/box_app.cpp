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
    createDefaultTexture();
    buildCbvSrvHeap();
    bindMaterialsToTextures();
    buildPso();
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
    mObjectCB = new UploadBuffer<ObjectConstants>(md3dDevice.Get(), 1, true);
    mPassCB = new UploadBuffer<PassConstants>(md3dDevice.Get(), 1, true);
}

void BoxApp::moveCamera(const GameTimer& gt) {
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
}

void BoxApp::update(const GameTimer& gt) {
    moveCamera(gt);

    XMVECTOR pos = XMLoadFloat3(&mEyePos);
    XMVECTOR target = pos + XMLoadFloat3(&mLook);
    XMVECTOR up = XMLoadFloat3(&mUp);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX viewProj = view * proj;

    mTextureOffset.x += mTextureScrollSpeedX;
    mTextureOffset.y += mTextureScrollSpeedY;
    if (mTextureOffset.x > 1.f) mTextureOffset.x -= 1.f;
    if (mTextureOffset.y > 1.f) mTextureOffset.y -= 1.f;

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(XMMatrixIdentity()));
    mObjectCB->copyData(0, objConstants);

    PassConstants passConstants;
    XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);
    XMStoreFloat4x4(&passConstants.InvViewProj, XMMatrixTranspose(invViewProj));
    passConstants.EyePosW = mEyePos;
    mPassCB->copyData(0, passConstants);
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

void BoxApp::draw(const GameTimer& gt) {
    failCheck(mDirectCmdListAlloc->Reset());
    failCheck(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPsoGeometry.Get()));

    mCommandList->RSSetViewports(1, &mViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Устанавливаем кучу
    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // --- GEOMETRY PASS ---
    mRenderingSystem->beginGeometryPass(mCommandList.Get(), getDepthStencilView(), getDepthStencilBuffer());

    // Устанавливаем константы (WorldViewProj)
    mCommandList->SetGraphicsRootConstantBufferView(0, mObjectCB->getResource()->GetGPUVirtualAddress());

    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    mCommandList->IASetIndexBuffer(&mIndexBufferView);

    for (const auto& submesh : mSubmeshes) {
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
        srvHandle.Offset(submesh.material.diffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        // register(t0) в geometry.hlsl
        mCommandList->SetGraphicsRootDescriptorTable(2, srvHandle);

        mCommandList->DrawIndexedInstanced(submesh.indexCount, 1, submesh.startIndiceIndex, 0, 0);
    }

    mRenderingSystem->endGeometryPass(mCommandList.Get(), getDepthStencilBuffer());

    // --- LIGHTING PASS ---
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(getCurrentBackBufferResource(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    // Очищаем бэк-буфер черным
    mCommandList->ClearRenderTargetView(getCurrentBackBuffer(), DirectX::Colors::Black, 0, nullptr);

    auto backBufferHandle = getCurrentBackBuffer();
    mCommandList->OMSetRenderTargets(1, &backBufferHandle, FALSE, nullptr);

    mCommandList->SetPipelineState(mPsoLighting.Get());

    // Константы камеры (InvViewProj)
    mCommandList->SetGraphicsRootConstantBufferView(1, mPassCB->getResource()->GetGPUVirtualAddress());

    // G-буфер (t1, t2, t3)
    mCommandList->SetGraphicsRootDescriptorTable(3, mGbufferSrvHandle);

    // Рисуем полноэкранный треугольник
    mCommandList->DrawInstanced(3, 1, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(getCurrentBackBufferResource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier);

    failCheck(mCommandList->Close());
    ID3D12CommandList* cmds[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(1, cmds);
    failCheck(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % swapChainBufferCount;
    flushCommandQueue();
}

void BoxApp::buildRootSignature() {
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);

    CD3DX12_DESCRIPTOR_RANGE texRange;
    texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    slotRootParameter[2].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_DESCRIPTOR_RANGE gbufferRange;
    gbufferRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1);
    slotRootParameter[3].InitAsDescriptorTable(1, &gbufferRange, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC linearWrap(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, 1, &linearWrap,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    failCheck(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob));
    failCheck(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::buildPso() {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC geoPsoDesc = {};
    geoPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    geoPsoDesc.pRootSignature = mRootSignature.Get();

    auto vs = D3DUtil::compileShader(L"geometry.hlsl", nullptr, "VS", "vs_5_0");
    auto ps = D3DUtil::compileShader(L"geometry.hlsl", nullptr, "PS", "ps_5_0");
    geoPsoDesc.VS = { (BYTE*)vs->GetBufferPointer(), vs->GetBufferSize() };
    geoPsoDesc.PS = { (BYTE*)ps->GetBufferPointer(), ps->GetBufferSize() };

    geoPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    geoPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    geoPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    geoPsoDesc.SampleMask = UINT_MAX;
    geoPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    geoPsoDesc.NumRenderTargets = 2;
    geoPsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    geoPsoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
    geoPsoDesc.DSVFormat = mDepthStencilFormat;
    geoPsoDesc.SampleDesc.Count = 1;

    failCheck(md3dDevice->CreateGraphicsPipelineState(&geoPsoDesc, IID_PPV_ARGS(&mPsoGeometry)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC lightPsoDesc = geoPsoDesc;
    lightPsoDesc.InputLayout = { nullptr, 0 };

    auto vsLight = D3DUtil::compileShader(L"lighting.hlsl", nullptr, "VS", "vs_5_0");
    auto psLight = D3DUtil::compileShader(L"lighting.hlsl", nullptr, "PS", "ps_5_0");
    lightPsoDesc.VS = { (BYTE*)vsLight->GetBufferPointer(), vsLight->GetBufferSize() };
    lightPsoDesc.PS = { (BYTE*)psLight->GetBufferPointer(), psLight->GetBufferSize() };

    lightPsoDesc.DepthStencilState.DepthEnable = FALSE;
    lightPsoDesc.NumRenderTargets = 1;
    lightPsoDesc.RTVFormats[0] = mBackBufferFormat;
    lightPsoDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;

    failCheck(md3dDevice->CreateGraphicsPipelineState(&lightPsoDesc, IID_PPV_ARGS(&mPsoLighting)));
}

void BoxApp::onResize() {
    D3DApp::onResize();

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, getAspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);

    if (mRenderingSystem) {
        mRenderingSystem->onResize(mClientWidth, mClientHeight);
        buildCbvSrvHeap();
    }
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
        texture->fileName = path.filename().wstring();
        texture->filePath = path.wstring();

        failCheck(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
            texture->filePath.c_str(), texture->resource, texture->uploadHeap));

        mTextures[texture->fileName] = std::move(texture);
    }
}

void BoxApp::buildCbvSrvHeap() {
    // 1. Считаем количество дескрипторов
    UINT numSceneTextures = (UINT)mTextures.size() + 1;
    UINT numGbufferTextures = 3; // Albedo, Normal, Depth
    UINT totalDescriptors = numSceneTextures + numGbufferTextures;

    // 2. Создаем кучу SRV (для шейдеров)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = totalDescriptors;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    failCheck(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mCbvSrvHeap)));

    // 3. Создаем кучу RTV для G-буфера (если она еще не создана или нужно обновить)
    // Именно здесь была ошибка nullptr
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2; // Albedo и Normal
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    failCheck(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mGbufferRtvHeap)));

    // 4. Заполняем кучу SRV текстурами сцены
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpu(mCbvSrvHeap->GetCPUDescriptorHandleForHeapStart());

    // Default Texture (белая)
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    md3dDevice->CreateShaderResourceView(mDefaultTex.Get(), &srvDesc, hCpu);

    // Текстуры модели
    UINT i = 1;
    for (auto& kv : mTextures) {
        auto texture = kv.second.get();
        srvDesc.Format = texture->resource->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = texture->resource->GetDesc().MipLevels;
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(hCpu, i, mCbvSrvDescriptorSize);
        md3dDevice->CreateShaderResourceView(texture->resource.Get(), &srvDesc, srvHandle);
        texture->srvHeapIndex = i++;
    }

    // 5. Инициализируем G-буфер в RenderingSystem
    if (!mRenderingSystem) {
        mRenderingSystem = std::make_unique<RenderingSystem>(md3dDevice.Get(), mClientWidth, mClientHeight);
    }

    // Сдвигаем хендлы к началу блока G-буфера в общей куче
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuGbuffer = hCpu;
    hCpuGbuffer.Offset(numSceneTextures, mCbvSrvDescriptorSize);

    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuGbuffer(mCbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
    hGpuGbuffer.Offset(numSceneTextures, mCbvSrvDescriptorSize);

    mGbufferSrvHandle = hGpuGbuffer;

    // Вызываем билд дескрипторов (теперь mGbufferRtvHeap точно не nullptr)
    mRenderingSystem->buildDescriptors(
        hCpuGbuffer,
        hGpuGbuffer,
        CD3DX12_CPU_DESCRIPTOR_HANDLE(mGbufferRtvHeap->GetCPUDescriptorHandleForHeapStart()),
        mCbvSrvDescriptorSize, mRtvDescriptorSize
    );

    // 6. Создаем SRV для Глубины (t3)
    // Сдвигаемся на 2 позиции (после Albedo и Normal)
    hCpuGbuffer.Offset(2, mCbvSrvDescriptorSize);

    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrvDesc.Texture2D.MipLevels = 1;

    md3dDevice->CreateShaderResourceView(getDepthStencilBuffer(), &depthSrvDesc, hCpuGbuffer);
}

void BoxApp::bindMaterialsToTextures() {
    for (auto& submesh : mSubmeshes) {
        const std::string& texName = submesh.material.diffuseTextureName;
        if (texName.empty()) {
            submesh.material.diffuseSrvHeapIndex = mDefaultTexIndex;
            continue;
        }
        std::wstring wTexName(texName.begin(), texName.end());
        auto it = mTextures.find(wTexName);
        submesh.material.diffuseSrvHeapIndex = (it != mTextures.end()) ? it->second->srvHeapIndex : mDefaultTexIndex;
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