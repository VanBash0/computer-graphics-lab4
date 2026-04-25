#ifndef BOX_APP_H
#define BOX_APP_H

#include "d3dapp.h"
#include "d3dutil.h"
#include "vertex.h"
#include "upload_buffer.h"
#include "model_loader.h"
#include "texture.h"
#include "rendering_system.h"
#include "octree.h"

#include <DirectXColors.h>
#include <DirectXMath.h>
#include <memory>

using namespace DirectX;

struct ObjectConstants {
    XMFLOAT4X4 WorldViewProj;
    XMFLOAT4X4 World;
    XMFLOAT4X4 TextureTransform;
    float TotalTime;
    float VertexAnimationEnabled;
    float TextureAnimationEnabled;
    float DisplacementScale;
    float MaxTessellationFactor;
    XMFLOAT3 Padding = { 0.0f, 0.0f, 0.0f };
};

struct PassConstants {
    XMFLOAT4X4 InvViewProj;
    XMFLOAT4X4 View;
    XMFLOAT4X4 Proj;
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

struct ParticleSimConstants {
    XMFLOAT3 EmitterPosition = { 0.0f, 10.0f, 0.0f };
    float InitialSize = 0.35f;
    XMFLOAT3 InitialVelocity = { 0.0f, 3.0f, 0.0f };
    float DeltaTime = 0.0f;
    XMFLOAT4 InitialColor = { 1.0f, 0.65f, 0.15f, 1.0f };
    XMFLOAT3 CameraPosition = { 0.0f, 0.0f, 0.0f };
    float TotalTime = 0.0f;
    float MinLifetime = 1.0f;
    float MaxLifetime = 3.0f;
    UINT EmitCount = 10;
    float Padding0 = 0.0f;
};

class BoxApp : public D3DApp {
public:
    void buildResources();
    void onResize() override;
    ~BoxApp();
    BoxApp(HINSTANCE hInstance) : D3DApp(hInstance) { initializeConstants(); };
private:
    static constexpr UINT PARTICLE_COUNT = 65536;
    static constexpr UINT PARTICLE_CS_GROUP_SIZE = 256;

    const float SPONZA_SCALE = 0.01f;
    const float EARTH_SCALE = 0.1f;
    const float SPEED_FACTOR = 10.f;
    const float DISPLACEMENT_SCALE = 0.4f;
    const float EARTH_BILLBOARD_SWITCH_DISTANCE = 60.0f;
    const float BILLBOARD_SIZE = 10.0f;
    const Vector3 TEXTURE_SCALE = Vector3(1.f, 1.f, 1.f);
    void setObjectSize(Vertex& vertex, float scale);

    void buildBuffers();
    void buildConstantBuffer();
    void buildRootSignature();
    void buildParticleRootSignature();
    void buildParticleComputeRootSignature();
    void buildLightingRootSignature();
    void buildPso(const std::wstring& shaderName, ComPtr<ID3D12PipelineState>& pso, bool enableTessellation = false);
    void buildParticlePso();
    void buildParticleResources();
    void buildParticleDescriptors();
    void dispatchParticlePass(const GameTimer& gt);
    void initializeConstants();
    void loadTextures();
    void buildCbvSrvHeap();
    void bindMaterialsToTextures();
    void buildOctree();
    std::vector<size_t> collectVisibleSubmeshes() const;

    UINT getPassCbvIndex() const;
    UINT getLightingCbvIndex() const;
    UINT getGBufferSrvStartIndex() const;
    UINT getDefaultTextureSrvStartIndex() const;
    UINT getParticlePoolSrvIndex() const;
    UINT getParticlePoolUavIndex() const;
    UINT getDeadListUavIndex() const;
    UINT getDeadListConsumeUavIndex() const;
    UINT getSortListUavIndex() const;

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
    UploadBuffer<ParticleSimConstants>* mParticleSimCB = nullptr;

    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12RootSignature> mParticleRootSignature;
    ComPtr<ID3D12RootSignature> mParticleComputeRootSignature;
    ComPtr<ID3D12RootSignature> mLightingRootSignature;
    ComPtr<ID3D12PipelineState> mPSO;
    ComPtr<ID3D12PipelineState> mEarthTessPSO;
    ComPtr<ID3D12PipelineState> mColumnPSO;
    ComPtr<ID3D12PipelineState> mLightingPSO;
    ComPtr<ID3D12PipelineState> mParticlePSO;
    ComPtr<ID3D12PipelineState> mParticleEmitPSO;
    ComPtr<ID3D12PipelineState> mParticleSimulatePSO;

    ComPtr<ID3D12DescriptorHeap> mCbvSrvHeap;
    UINT mCbvSrvDescriptorSize = 0;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    XMFLOAT4X4 mWorld;
    XMFLOAT4X4 mView;
    XMFLOAT4X4 mProj;

    DirectX::XMFLOAT3 mEyePos = { 0.0f, 12.0f, -20.0f };
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
    std::vector<XMFLOAT4X4> mSubmeshWorlds;
    Octree mSceneOctree;
    std::vector<LightData> mLights;
    std::vector<SwingingSpotLight> mSwingingSpotLights;
    std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
    ComPtr<ID3D12Resource> mDefaultDiffuseTex = nullptr;
    ComPtr<ID3D12Resource> mDefaultNormalTex = nullptr;
    ComPtr<ID3D12Resource> mDefaultDisplacementTex = nullptr;
    ComPtr<ID3D12Resource> mDefaultDiffuseTexUpload = nullptr;
    ComPtr<ID3D12Resource> mDefaultNormalTexUpload = nullptr;
    ComPtr<ID3D12Resource> mDefaultDisplacementTexUpload = nullptr;
    std::unique_ptr<RenderingSystem> mRenderingSystem;
    ComPtr<ID3D12Resource> mParticlePoolBuffer;
    ComPtr<ID3D12Resource> mParticlePoolUpload;
    ComPtr<ID3D12Resource> mDeadListBuffer;
    ComPtr<ID3D12Resource> mDeadListUpload;
    ComPtr<ID3D12Resource> mDeadListCounterBuffer;
    ComPtr<ID3D12Resource> mDeadListCounterUpload;
    ComPtr<ID3D12Resource> mSortListBuffer;
    ComPtr<ID3D12Resource> mParticleIndexBuffer;
    ComPtr<ID3D12Resource> mParticleIndexUpload;
    D3D12_VERTEX_BUFFER_VIEW mParticleIndexBufferView = {};

    size_t mBillboardIndex = static_cast<size_t>(-1);
    DirectX::XMFLOAT3 mEarthPosition = { 0.0f, 12.0f, 0.0f };
    DirectX::XMFLOAT3 mEarthBillboardPosition = { 0.0f, 24.0f, 0.0f };
    std::vector<size_t> mEarthSubmeshIndices;

    bool mEnableColumnVertexAnimation = true;
    bool mEnableColumnTextureAnimation = true;
    bool mEnableFrustumCulling = true;
};

#endif // BOX_APP_H