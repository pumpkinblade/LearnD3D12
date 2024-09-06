#include "SimpleComputeApp.h"

void SimpleComputeApp::OnInit() {
  ComputeApp::OnInit();

  // create resource
  mVectorLength = 10;
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mVectorLength * sizeof(float)),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mInputBuffer1)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mVectorLength * sizeof(float)),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mInputBuffer2)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mVectorLength * sizeof(float),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(&mOutputBuffer)));

  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mVectorLength * sizeof(float)),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mUploadBuffer1)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mVectorLength * sizeof(float)),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mUploadBuffer2)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mVectorLength * sizeof(float)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mReadbackBuffer)));

  // Create RootSignature
  CD3DX12_ROOT_PARAMETER rootParams[4];
  rootParams[0].InitAsConstants(2, 0);
  rootParams[1].InitAsShaderResourceView(0);
  rootParams[2].InitAsShaderResourceView(1);
  rootParams[3].InitAsUnorderedAccessView(0);
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(_countof(rootParams), rootParams);
  ComPtr<ID3DBlob> signature, error;
  ThrowIfFailed(D3D12SerializeRootSignature(
      &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
  ThrowIfFailed(mDevice->CreateRootSignature(0, signature->GetBufferPointer(),
                                             signature->GetBufferSize(),
                                             IID_PPV_ARGS(&mRootSignature)));

  // Create Shader
  mThreadNum = 16;
  char threadNumBuf[16];
  std::snprintf(threadNumBuf, 16, "%d", mThreadNum);
  D3D_SHADER_MACRO macros[2] = {{"THREAD_NUM", threadNumBuf},
                                {nullptr, nullptr}};
  mShader =
      d3dUtil::CompileShader(L"SimpleCompute.hlsl", macros, "main", "cs_5_0");

  // Create PipelineState
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = mRootSignature.Get();
  psoDesc.CS = {reinterpret_cast<BYTE *>(mShader->GetBufferPointer()),
                mShader->GetBufferSize()};
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mPipelineState)));
}

void SimpleComputeApp::OnCompute() {
  ComputeApp::OnCompute();

  std::vector<float> in1(mVectorLength);
  std::vector<float> in2(mVectorLength);
  std::vector<float> out(mVectorLength);
  float alpha = 10.f;
  for (UINT i = 0; i < mVectorLength; i++) {
    in1[i] = static_cast<float>(i);
    in2[i] = static_cast<float>(i);
  }

  // copy to gpu
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mUploadBuffer1->Map(0, nullptr, &mappedData));
    memcpy(mappedData, in1.data(), mVectorLength * sizeof(float));
    mUploadBuffer1->Unmap(0, nullptr);
  }
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mUploadBuffer2->Map(0, nullptr, &mappedData));
    memcpy(mappedData, in2.data(), mVectorLength * sizeof(float));
    mUploadBuffer2->Unmap(0, nullptr);
  }

  ThrowIfFailed(mCmdAlloc->Reset());
  ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), mPipelineState.Get()));

  // upload
  //mCmdList->ResourceBarrier(
  //    1, &CD3DX12_RESOURCE_BARRIER::Transition(mInputBuffer1.Get(),
  //                                             D3D12_RESOURCE_STATE_COMMON,
  //                                             D3D12_RESOURCE_STATE_COPY_DEST));
  mCmdList->CopyResource(mInputBuffer1.Get(), mUploadBuffer1.Get());
  //mCmdList->ResourceBarrier(
  //    1, &CD3DX12_RESOURCE_BARRIER::Transition(mInputBuffer1.Get(),
  //                                             D3D12_RESOURCE_STATE_COPY_DEST,
  //                                             D3D12_RESOURCE_STATE_COMMON));
  //mCmdList->ResourceBarrier(
  //    1, &CD3DX12_RESOURCE_BARRIER::Transition(mInputBuffer2.Get(),
  //                                             D3D12_RESOURCE_STATE_COMMON,
  //                                             D3D12_RESOURCE_STATE_COPY_DEST));
  mCmdList->CopyResource(mInputBuffer2.Get(), mUploadBuffer2.Get());
  //mCmdList->ResourceBarrier(
  //    1, &CD3DX12_RESOURCE_BARRIER::Transition(mInputBuffer2.Get(),
  //                                             D3D12_RESOURCE_STATE_COPY_DEST,
  //                                             D3D12_RESOURCE_STATE_COMMON));

  // dispatch
  mCmdList->SetComputeRootSignature(mRootSignature.Get());
  mCmdList->SetComputeRoot32BitConstant(0, *(UINT *)&alpha, 0);
  mCmdList->SetComputeRoot32BitConstant(0, *(UINT *)&mVectorLength, 1);
  mCmdList->SetComputeRootShaderResourceView(
      1, mInputBuffer1->GetGPUVirtualAddress());
  mCmdList->SetComputeRootShaderResourceView(
      2, mInputBuffer2->GetGPUVirtualAddress());
  mCmdList->SetComputeRootUnorderedAccessView(
      3, mOutputBuffer->GetGPUVirtualAddress());
  mCmdList->Dispatch((mVectorLength + mThreadNum - 1) / mThreadNum, 1, 1);

  // download
  mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                                   mOutputBuffer.Get(),
                                   D3D12_RESOURCE_STATE_COMMON,
                                   D3D12_RESOURCE_STATE_COPY_SOURCE));
  mCmdList->CopyResource(mReadbackBuffer.Get(), mOutputBuffer.Get());
  //mCmdList->ResourceBarrier(
  //    1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
  //                                             D3D12_RESOURCE_STATE_COPY_SOURCE,
  //                                             D3D12_RESOURCE_STATE_COMMON));

  ThrowIfFailed(mCmdList->Close());
  ID3D12CommandList *cmdLists[] = {mCmdList.Get()};
  mCmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
  FlushCommandQueue();

  // copy to cpu
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mReadbackBuffer->Map(0, nullptr, &mappedData));
    memcpy(out.data(), mappedData, mVectorLength * sizeof(float));
    mReadbackBuffer->Unmap(0, nullptr);
  }

  // log the result
  for (UINT i = 0; i < mVectorLength; i++) {
    std::printf("%f + %f * %f = %f\n", in1[i], alpha, in2[i], out[i]);
  }
}
