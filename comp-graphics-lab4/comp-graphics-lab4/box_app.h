#ifndef BOX_APP_H
#define BOX_APP_H

#include "d3dapp.h"
#include "d3dutil.h"
#include "vertex.h"
#include <DirectXColors.h>
#include "upload_buffer.h"
#include <DirectXMath.h>

using namespace DirectX;

struct ObjectConstants {
    XMFLOAT4X4 WorldViewProj;
    XMFLOAT4X4 World;
};

class BoxApp : public D3DApp {
public:
    void buildResources();
    void onResize() override;
    ~BoxApp();
    BoxApp(HINSTANCE hInstance) : D3DApp(hInstance) { initializeConstants(); };
private:
    const float SCENE_SCALE = 0.01f;
    const float SPEED_FACTOR = 10.f;
    void setObjectSize(Vertex& vertex, float scale);

    void buildBuffers();
    void buildConstantBuffer();
    void buildCbv();
    void buildRootSignature();
    void buildPso();
    void initializeConstants();

    void update(const GameTimer& gt) override;
    void draw(const GameTimer& gt) override;
    void onMouseMove(WPARAM btnState, int x, int y) override;

    ComPtr<ID3D12Resource> mVertexBufferGPU;
    ComPtr<ID3D12Resource> mVertexBufferUploader;

    ComPtr<ID3D12Resource> mIndexBufferGPU;
    ComPtr<ID3D12Resource> mIndexBufferUploader;

    UploadBuffer<ObjectConstants>* mObjectCB = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap;
    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12PipelineState> mPSO;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    XMFLOAT4X4 mWorld;
    XMFLOAT4X4 mView;
    XMFLOAT4X4 mProj;

    DirectX::XMFLOAT3 mEyePos = { 0.0f, 2.0f, -10.0f };
    DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };

    float mYaw = 0.0f;
    float mPitch = 0.0f;

    POINT mLastMousePos;

    UINT mIndexCount = 0;

    D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

    std::vector<MeshData> mMeshes;
};

#endif // BOX_APP_H