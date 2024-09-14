#pragma once
#include <Common/ComputeApp.h>

class BasicGamer2DApp : public ComputeApp {
public:
  void OnInit() override;
  void OnCompute() override;

private:
  void CreateBuffer();
  void CreateRootSignature();
  void CreateComputeShader();
  void CreatePipelineState();

  UINT mRow, mCol;
  UINT mMaxTurns;
  UINT mNumPins;

  ComPtr<ID3D12Resource> mCostHorizontalBuffer;
  ComPtr<ID3D12Resource> mCostVerticalBuffer;
  ComPtr<ID3D12Resource> mDistBuffer;
  ComPtr<ID3D12Resource> mAllPrevBuffer;
  ComPtr<ID3D12Resource> mMarkBuffer;
  ComPtr<ID3D12Resource> mRoutesBuffer;
  ComPtr<ID3D12Resource> mIsRoutedPinBuffer;
  ComPtr<ID3D12Resource> mPinIndicesBuffer;

  // UploadBuffer for CostHorizontal, CostVertical, Mark, PinIndices
  ComPtr<ID3D12Resource> mCostHorizontalUploadBuffer;
  ComPtr<ID3D12Resource> mCostVerticalUploadBuffer;
  ComPtr<ID3D12Resource> mMarkUploadBuffer;
  ComPtr<ID3D12Resource> mPinIndicesUploadBuffer;

  // ReadbackBuffer for Routes
  ComPtr<ID3D12Resource> mRoutesReadbackBuffer;
  ComPtr<ID3D12Resource> mDistReadbackBuffer;
  ComPtr<ID3D12Resource> mAllPrevReadbackBuffer;
  ComPtr<ID3D12Resource> mMarkReadbackBuffer;

  // Sweep
  ComPtr<ID3D12RootSignature> mSweepHorizontalRootSignature;
  ComPtr<ID3D12RootSignature> mSweepVerticalRootSignature;
  ComPtr<ID3DBlob> mSweepHorizontalCS;
  ComPtr<ID3DBlob> mSweepVerticalCS;
  ComPtr<ID3D12PipelineState> mSweepHorizontalPSO;
  ComPtr<ID3D12PipelineState> mSweepVerticalPSO;

  // CleanDist
  ComPtr<ID3D12RootSignature> mCleanDistRootSignature;
  ComPtr<ID3DBlob> mCleanDistCS;
  ComPtr<ID3D12PipelineState> mCleanDistPSO;

  // SetRootPin
  ComPtr<ID3D12RootSignature> mSetRootPinRootSignature;
  ComPtr<ID3DBlob> mSetRootPinCS;
  ComPtr<ID3D12PipelineState> mSetRootPinPSO;

  // TracePath
  ComPtr<ID3D12RootSignature> mTracePathRootSignature;
  ComPtr<ID3DBlob> mTracePathCS;
  ComPtr<ID3D12PipelineState> mTracePathPSO;
};
