#pragma once

#include "Common/d3dUtil.h"
#include "Common/GameTimer.h"

class WaveSimulator
{
public:
    WaveSimulator(ID3D12Device* device, int m, int n, float dx, float dt, float speed, float damping);
    WaveSimulator(const WaveSimulator& rhs) = delete;
    WaveSimulator& operator=(const WaveSimulator& rhs) = delete;
    ~WaveSimulator() = default;

    int RowCount() const { return mNumRows; }
    int ColumnCount() const { return mNumCols; }
    int VertexCount() const { return mVertexCount; }
    int TriangleCount() const { return mTriangleCount; }
    float Width() const { return mNumCols * mSpatialStep; }
    float Depth() const { return mNumRows * mSpatialStep; }
    float SpatialStep() const { return mSpatialStep; }

    static const UINT DescriptorCount() { return 6; }

    void Update(const GameTimer& gt, ID3D12GraphicsCommandList* cmdList);
    void Disturb(ID3D12GraphicsCommandList* cmdList, int i, int j, float magnitude);
    CD3DX12_GPU_DESCRIPTOR_HANDLE DispatchmentMap();

    void Initialze(ID3D12GraphicsCommandList* cmdList, CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize);
private:
    void BuildResources(ID3D12GraphicsCommandList* cmdList);
    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize);
    void BuildRootSignature();
    void BuildPSOs();
private:
    int mNumRows = 0;
    int mNumCols = 0;

    int mVertexCount = 0;
    int mTriangleCount = 0;

    // Simulation constants we can precompute.
    float mK1 = 0.0f;
    float mK2 = 0.0f;
    float mK3 = 0.0f;

    float mTimeStep = 0.0f;
    float mSpatialStep = 0.0f;

    ID3D12Device* md3dDevice = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mUpdatePSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mDisturbPSO;
    Microsoft::WRL::ComPtr<ID3DBlob> mUpdateCS;
    Microsoft::WRL::ComPtr<ID3DBlob> mDisturbCS;

    Microsoft::WRL::ComPtr<ID3D12Resource> mPrevSln = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> mCurrSln = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> mNextSln = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer = nullptr;

    CD3DX12_GPU_DESCRIPTOR_HANDLE mPrevSlnSrvView;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mCurrSlnSrvView;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mNextSlnSrvView;

    CD3DX12_GPU_DESCRIPTOR_HANDLE mPrevSlnUavView;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mCurrSlnUavView;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mNextSlnUavView;
};