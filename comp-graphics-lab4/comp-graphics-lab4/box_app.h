#ifndef BOX_APP_H
#define BOX_APP_H

#include "d3dapp.h"
#include "d3dutil.h"
#include "vertex.h"
#include "upload_buffer.h"
#include "model_loader.h"
#include "texture.h"
#include "rendering_system.h"

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <memory>

using namespace DirectX;

struct ObjectConstants {
    XMFLOAT4X4 WorldViewProj;
    XMFLOAT4X4 World;
    XMFLOAT4X4 TextureTransform;
    float TotalTime;
    XMFLOAT3 Padding;
};

struct PassConstants {
    XMFLOAT4X4 InvViewProj;
    XMFLOAT3 EyePosW;
    float Padding = 0.0f;
    XMFLOAT4 AmbientColor;
};

enum class LightType : UINT {
    Point = 0,
    Directional = 1,
    Spot = 2
};

constexpr UINT MAX_LIGHTS = 16;

struct LightData {
    XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    float Range = 1.0f;

    XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;

    XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
    UINT Type = static_cast<UINT>(LightType::Point);

    XMFLOAT3 Attenuation = { 1.0f, 0.0f, 1.0f };
    float SpotAngle = 0.0f;
};

struct LightingConstants {
    UINT LightCount = 0;
    XMFLOAT3 Padding = { 0.0f, 0.0f, 0.0f };
    LightData Lights[MAX_LIGHTS];
};

struct SwingingSpotLight {
    LightData Light;
    XMFLOAT3 AnchorPosition = { 0.0f, 0.0f, 0.0f };
    float SpawnTime = 0.0f;
    float PhaseOffset = 0.0f;
};

class BoxApp : public D3DApp {
public:
    void buildResources();
    void onResize() override;
    ~BoxApp();
    BoxApp(HINSTANCE hInstance) : D3DApp(hInstance) { initializeConstants(); };
private:
    const float SPONZA_SCALE = 0.01f;
    const float EARTH_SCALE = 0.1f;
    const float SPEED_FACTOR = 10.f;
    const Vector3 TEXTURE_SCALE = Vector3(1.f, 1.f, 1.f);
    void setObjectSize(Vertex& vertex, float scale);

    void buildBuffers();
    void buildConstantBuffer();
    void buildRootSignature();
    void buildLightingRootSignature();
    void buildPso(const std::wstring& shaderName, ComPtr<ID3D12PipelineState>& pso);
    void initializeConstants();
    void loadTextures();
    void buildCbvSrvHeap();
    void bindMaterialsToTextures();

    void update(const GameTimer& gt) override;
    void draw(const GameTimer& gt) override;
    void onMouseMove(WPARAM btnState, int x, int y) override;

    void createDefaultTextures();

    ComPtr<ID3D12Resource> mVertexBufferGPU;
    ComPtr<ID3D12Resource> mVertexBufferUploader;


    ComPtr<ID3D12Resource> mIndexBufferGPU;
    ComPtr<ID3D12Resource> mIndexBufferUploader;

    UploadBuffer<ObjectConstants>* mObjectCB = nullptr;
    UploadBuffer<PassConstants>* mPassCB = nullptr;
    UploadBuffer<LightingConstants>* mLightingCB = nullptr;

    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12RootSignature> mLightingRootSignature;
    ComPtr<ID3D12PipelineState> mPSO;
    ComPtr<ID3D12PipelineState> mColumnPSO;
    ComPtr<ID3D12PipelineState> mLightingPSO;

    ComPtr<ID3D12DescriptorHeap> mCbvSrvHeap;
    UINT mCbvSrvDescriptorSize = 0;

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

    std::vector<Submesh> mSubmeshes;
    std::vector<LightData> mLights;
    std::vector<SwingingSpotLight> mSwingingSpotLights;
    std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
    ComPtr<ID3D12Resource> mDefaultDiffuseTex = nullptr;
    ComPtr<ID3D12Resource> mDefaultNormalTex = nullptr;
    ComPtr<ID3D12Resource> mDefaultDiffuseTexUpload = nullptr;
    ComPtr<ID3D12Resource> mDefaultNormalTexUpload = nullptr;
    std::unique_ptr<RenderingSystem> mRenderingSystem;

    bool mEnableColumnVertexAnimation = true;
    bool mEnableColumnTextureAnimation = true;
};

#endif // BOX_APP_H