#include "BitonicSortApp.h"
#include <random>

void BitonicSortApp::OnInit() {
  ComputeApp::OnInit();

  // create resource
  mVectorLength = 32;
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mVectorLength * sizeof(float),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
      IID_PPV_ARGS(mComputeBuffer.ReleaseAndGetAddressOf())));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mVectorLength * sizeof(float)),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(mUploadBuffer.ReleaseAndGetAddressOf())));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mVectorLength * sizeof(float)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(mReadbackBuffer.ReleaseAndGetAddressOf())));

  // create RootSignature
  CD3DX12_ROOT_PARAMETER rootParams[1];
  rootParams[0].InitAsUnorderedAccessView(0);
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
  rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr,
                   D3D12_ROOT_SIGNATURE_FLAG_NONE);
  ComPtr<ID3DBlob> signature, error;
  ThrowIfFailed(D3D12SerializeRootSignature(
      &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
  ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(),
                                             signature->GetBufferSize(),
                                             IID_PPV_ARGS(&mRootSignature)));

  // create Shader
  char vectorLengthBuf[16];
  std::snprintf(vectorLengthBuf, 16, "%u", mVectorLength);
  D3D_SHADER_MACRO macro[2] = {{"VECTOR_LENGTH", vectorLengthBuf},
                               {nullptr, nullptr}};
  mShader =
      d3dUtil::CompileShader(L"BitonicSort.hlsl", macro, "main", "cs_5_0");

  // create PipelineState
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = mRootSignature.Get();
  psoDesc.CS = {reinterpret_cast<BYTE *>(mShader->GetBufferPointer()),
                mShader->GetBufferSize()};
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mPipelineState)));
}

void BitonicSortApp::OnCompute() {
  ComputeApp::OnCompute();

  std::vector<int> beforeSort(mVectorLength);
  std::vector<int> afterSort(mVectorLength);
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> uni(0, 9);
  for (UINT i = 0; i < mVectorLength; i++)
    beforeSort[i] = uni(rng);

  // copy to gpu
  {
    int *mappedData = nullptr;
    ThrowIfFailed(
        mUploadBuffer->Map(0, nullptr, reinterpret_cast<void **>(&mappedData)));
    memcpy(mappedData, beforeSort.data(), mVectorLength * sizeof(int));
    mUploadBuffer->Unmap(0, nullptr);
  }

  ThrowIfFailed(mCmdAlloc->Reset());
  ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), mPipelineState.Get()));

  // upload
  mCmdList->CopyResource(mComputeBuffer.Get(), mUploadBuffer.Get());

  // dispatch
  mCmdList->SetComputeRootSignature(mRootSignature.Get());
  mCmdList->SetComputeRootUnorderedAccessView(
      0, mComputeBuffer->GetGPUVirtualAddress());
  mCmdList->Dispatch(1, 1, 1);

  // download
  mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                                   mComputeBuffer.Get(),
                                   D3D12_RESOURCE_STATE_COMMON,
                                   D3D12_RESOURCE_STATE_COPY_SOURCE));
  mCmdList->CopyResource(mReadbackBuffer.Get(), mComputeBuffer.Get());

  ThrowIfFailed(mCmdList->Close());
  ID3D12CommandList *cmdLists[] = {mCmdList.Get()};
  mCmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
  FlushCommandQueue();

  // copy to cpu
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mReadbackBuffer->Map(0, nullptr, &mappedData));
    memcpy(afterSort.data(), mappedData, mVectorLength * sizeof(int));
    mReadbackBuffer->Unmap(0, nullptr);
  }

  // log result
  std::printf("before sort: \n");
  for (UINT i = 0; i < mVectorLength; i++) {
    std::printf("%d ", beforeSort[i]);
  }
  std::printf("\nafter sort: \n");
  for (UINT i = 0; i < mVectorLength; i++) {
    std::printf("%d ", afterSort[i]);
  }
}
