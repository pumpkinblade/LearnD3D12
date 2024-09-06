#pragma once

#include <Common/ComputeApp.h>
#include <vector>

class SimpleComputeApp : public ComputeApp
{
protected:
  void OnInit() override;
  void OnCompute() override;

private:
  UINT mThreadNum;
  ComPtr<ID3D12RootSignature> mRootSignature;
  ComPtr<ID3DBlob> mShader;
  ComPtr<ID3D12PipelineState> mPipelineState;

  UINT mVectorLength;
  ComPtr<ID3D12Resource> mInputBuffer1;
  ComPtr<ID3D12Resource> mInputBuffer2;
  ComPtr<ID3D12Resource> mOutputBuffer;
  ComPtr<ID3D12Resource> mUploadBuffer1;
  ComPtr<ID3D12Resource> mUploadBuffer2;
  ComPtr<ID3D12Resource> mReadbackBuffer;
};
