#ifndef UPLOAD_BUFFER_H
#define UPLOAD_BUFFER_H

#include "d3dutil.h"
#include "fail_checker.h"

template<typename T>
class UploadBuffer {
public:
    UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer) {
        mElementByteSize = sizeof(T);
        if (mIsConstantBuffer) mElementByteSize = D3DUtil::calcConstantBufferByteSize(sizeof(T));
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
        failCheck(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mUploadBuffer)
        ));
        failCheck(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
    }

    ~UploadBuffer()
    {
        if (mUploadBuffer != nullptr)
            mUploadBuffer->Unmap(0, nullptr);

        mMappedData = nullptr;
    }
    
    void copyData(int elementIndex, const T& data) {
        memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
    }

    ID3D12Resource* getResource() const { return mUploadBuffer.Get(); }

    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

private:
    bool mIsConstantBuffer = false;
    UINT mElementByteSize = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
    BYTE* mMappedData = nullptr;
};

#endif // UPLOAD_BUFFER_H