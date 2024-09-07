#pragma once

#include <vector>
#include <Common/ComputeApp.h>

class PrefixSumApp : public ComputeApp {
public:
  void OnInit() override;
  void OnCompute() override;

private:
  ComPtr<ID3D12RootSignature> mRootSignature;
  ComPtr<ID3DBlob> mShader;
  ComPtr<ID3D12PipelineState> mPipelineState;

  UINT mVectorLength;
  ComPtr<ID3D12Resource> mComputeBuffer;
  ComPtr<ID3D12Resource> mUploadBuffer;
  ComPtr<ID3D12Resource> mReadbackBuffer;
};
