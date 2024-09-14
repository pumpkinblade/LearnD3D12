#include "BasicGamer2DApp.h"

void BasicGamer2DApp::OnInit() {
  ComputeApp::OnInit();

  mRow = mCol = 4;
  mNumPins = 3;
  mMaxTurns = 4;

  CreateBuffer();
  CreateRootSignature();
  CreateComputeShader();
  CreatePipelineState();
}

void BasicGamer2DApp::OnCompute() {
  ComputeApp::OnCompute();

  std::vector<float> costHorizontal(mRow * mCol);
  costHorizontal[0 + 0 * mRow] = 0;
  costHorizontal[1 + 0 * mRow] = 1;
  costHorizontal[2 + 0 * mRow] = 3;
  costHorizontal[3 + 0 * mRow] = 1000;
  costHorizontal[0 + 1 * mRow] = 0;
  costHorizontal[1 + 1 * mRow] = 2;
  costHorizontal[2 + 1 * mRow] = 7;
  costHorizontal[3 + 1 * mRow] = 1000;
  costHorizontal[0 + 2 * mRow] = 0;
  costHorizontal[1 + 2 * mRow] = 5;
  costHorizontal[2 + 2 * mRow] = 4;
  costHorizontal[3 + 2 * mRow] = 1000;
  std::vector<float> costVertical(mRow * mCol);
  costVertical[0 + 0 * mRow] = 0;
  costVertical[0 + 1 * mRow] = 9;
  costVertical[0 + 2 * mRow] = 3;
  costVertical[0 + 3 * mRow] = 1000;
  costVertical[1 + 0 * mRow] = 0;
  costVertical[1 + 1 * mRow] = 5;
  costVertical[1 + 2 * mRow] = 7;
  costVertical[1 + 3 * mRow] = 1000;
  costVertical[2 + 0 * mRow] = 0;
  costVertical[2 + 1 * mRow] = 1;
  costVertical[2 + 2 * mRow] = 2;
  costVertical[2 + 3 * mRow] = 1000;
  std::vector<UINT> mark(mRow * mCol, 0);
  std::vector<UINT> pinIndices = {0 * mRow + 0, 1 * mRow + 2, 2 * mRow + 0};

  // copy to gpu
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mCostHorizontalUploadBuffer->Map(0, nullptr, &mappedData));
    memcpy(mappedData, costHorizontal.data(), mRow * mCol * sizeof(float));
    mCostHorizontalUploadBuffer->Unmap(0, nullptr);
  }
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mCostVerticalUploadBuffer->Map(0, nullptr, &mappedData));
    memcpy(mappedData, costVertical.data(), mRow * mCol * sizeof(float));
    mCostVerticalUploadBuffer->Unmap(0, nullptr);
  }
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mMarkUploadBuffer->Map(0, nullptr, &mappedData));
    memcpy(mappedData, mark.data(), mRow * mCol * sizeof(float));
    mMarkUploadBuffer->Unmap(0, nullptr);
  }
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mPinIndicesUploadBuffer->Map(0, nullptr, &mappedData));
    memcpy(mappedData, pinIndices.data(), pinIndices.size() * sizeof(UINT));
    mPinIndicesUploadBuffer->Unmap(0, nullptr);
  }

  // Reset cmdList
  ThrowIfFailed(mCmdAlloc->Reset());
  ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), nullptr));

  std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

  // upload
  barriers.clear();
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mCostHorizontalBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COPY_DEST));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mCostVerticalBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COPY_DEST));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mMarkBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COPY_DEST));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mPinIndicesBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COPY_DEST));
  mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
  mCmdList->CopyResource(mCostHorizontalBuffer.Get(),
                         mCostHorizontalUploadBuffer.Get());
  mCmdList->CopyResource(mCostVerticalBuffer.Get(),
                         mCostVerticalUploadBuffer.Get());
  mCmdList->CopyResource(mMarkBuffer.Get(), mMarkUploadBuffer.Get());
  mCmdList->CopyResource(mPinIndicesBuffer.Get(),
                         mPinIndicesUploadBuffer.Get());
  barriers.clear();
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mCostHorizontalBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_COMMON));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mCostVerticalBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_COMMON));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mMarkBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_COMMON));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mPinIndicesBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_COMMON));
  mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());

  // SetRootPin
  mCmdList->SetPipelineState(mSetRootPinPSO.Get());
  mCmdList->SetComputeRootSignature(mSetRootPinRootSignature.Get());
  mCmdList->SetComputeRoot32BitConstant(0, mNumPins, 0);
  mCmdList->SetComputeRootUnorderedAccessView(
      1, mPinIndicesBuffer->GetGPUVirtualAddress());
  mCmdList->SetComputeRootUnorderedAccessView(
      2, mMarkBuffer->GetGPUVirtualAddress());
  mCmdList->SetComputeRootUnorderedAccessView(
      3, mIsRoutedPinBuffer->GetGPUVirtualAddress());
  mCmdList->SetComputeRootUnorderedAccessView(
      4, mRoutesBuffer->GetGPUVirtualAddress());
  mCmdList->Dispatch(1, 1, 1);

  // gamer process
  for (UINT i = 1; i < pinIndices.size(); i++) {
    // CleanDist
    barriers.clear();
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mMarkBuffer.Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mDistBuffer.Get()));
    mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
    mCmdList->SetPipelineState(mCleanDistPSO.Get());
    mCmdList->SetComputeRootSignature(mCleanDistRootSignature.Get());
    mCmdList->SetComputeRootUnorderedAccessView(
        0, mMarkBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        1, mDistBuffer->GetGPUVirtualAddress());
    mCmdList->Dispatch(mCol, 1, 1);
    for (UINT turn = 0; turn < mMaxTurns; turn++) {
      barriers.clear();
      barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mDistBuffer.Get()));
      barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mAllPrevBuffer.Get()));
      mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
      if ((turn & 1)) {
        // SweepVertical
        mCmdList->SetPipelineState(mSweepVerticalPSO.Get());
        mCmdList->SetComputeRootSignature(mSweepVerticalRootSignature.Get());
        mCmdList->SetComputeRoot32BitConstant(0, turn, 0);
        mCmdList->SetComputeRootUnorderedAccessView(
            1, mCostVerticalBuffer->GetGPUVirtualAddress());
        mCmdList->SetComputeRootUnorderedAccessView(
            2, mDistBuffer->GetGPUVirtualAddress());
        mCmdList->SetComputeRootUnorderedAccessView(
            3, mAllPrevBuffer->GetGPUVirtualAddress());
        mCmdList->Dispatch(mRow, 1, 1);
      } else {
        // SweepHorizontal
        mCmdList->SetPipelineState(mSweepHorizontalPSO.Get());
        mCmdList->SetComputeRootSignature(mSweepHorizontalRootSignature.Get());
        mCmdList->SetComputeRoot32BitConstant(0, turn, 0);
        mCmdList->SetComputeRootUnorderedAccessView(
            1, mCostHorizontalBuffer->GetGPUVirtualAddress());
        mCmdList->SetComputeRootUnorderedAccessView(
            2, mDistBuffer->GetGPUVirtualAddress());
        mCmdList->SetComputeRootUnorderedAccessView(
            3, mAllPrevBuffer->GetGPUVirtualAddress());
        mCmdList->Dispatch(mCol, 1, 1);
      }
    }
    // TracePath
    barriers.clear();
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mDistBuffer.Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mAllPrevBuffer.Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mPinIndicesBuffer.Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mMarkBuffer.Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mIsRoutedPinBuffer.Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mRoutesBuffer.Get()));
    mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
    mCmdList->SetPipelineState(mTracePathPSO.Get());
    mCmdList->SetComputeRootSignature(mTracePathRootSignature.Get());
    mCmdList->SetComputeRoot32BitConstant(0, mNumPins, 0);
    mCmdList->SetComputeRoot32BitConstant(0, mMaxTurns, 1);
    mCmdList->SetComputeRootUnorderedAccessView(
        1, mDistBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        2, mAllPrevBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        3, mPinIndicesBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        4, mMarkBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        5, mIsRoutedPinBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        6, mRoutesBuffer->GetGPUVirtualAddress());
    mCmdList->Dispatch(1, 1, 1);
  }

  // download
  barriers.clear();
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mRoutesBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COPY_SOURCE));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mDistBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COPY_SOURCE));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mAllPrevBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COPY_SOURCE));
  barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
      mMarkBuffer.Get(), D3D12_RESOURCE_STATE_COMMON,
      D3D12_RESOURCE_STATE_COPY_SOURCE));
  mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
  mCmdList->CopyResource(mRoutesReadbackBuffer.Get(), mRoutesBuffer.Get());
  mCmdList->CopyResource(mDistReadbackBuffer.Get(), mDistBuffer.Get());
  mCmdList->CopyResource(mAllPrevReadbackBuffer.Get(), mAllPrevBuffer.Get());
  mCmdList->CopyResource(mMarkReadbackBuffer.Get(), mMarkBuffer.Get());

  // commit cmdList
  ThrowIfFailed(mCmdList->Close());
  ID3D12CommandList *cmdLists[] = {mCmdList.Get()};
  mCmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
  FlushCommandQueue();

  // log result
  std::vector<int> routes(2 * mNumPins * mMaxTurns);
  std::vector<float> dist(mRow * mCol);
  std::vector<int> allPrev(mMaxTurns * mRow * mCol);
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mRoutesReadbackBuffer->Map(0, nullptr, &mappedData));
    memcpy(routes.data(), mappedData, routes.size() * sizeof(int));
    mRoutesReadbackBuffer->Unmap(0, nullptr);
  }
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mDistReadbackBuffer->Map(0, nullptr, &mappedData));
    memcpy(dist.data(), mappedData, dist.size() * sizeof(float));
    mDistReadbackBuffer->Unmap(0, nullptr);
  }
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mAllPrevReadbackBuffer->Map(0, nullptr, &mappedData));
    memcpy(allPrev.data(), mappedData, allPrev.size() * sizeof(int));
    mAllPrevReadbackBuffer->Unmap(0, nullptr);
  }
  {
    void *mappedData = nullptr;
    ThrowIfFailed(mMarkReadbackBuffer->Map(0, nullptr, &mappedData));
    memcpy(mark.data(), mappedData, mark.size() * sizeof(int));
    mMarkReadbackBuffer->Unmap(0, nullptr);
  }
  for (UINT rowId = 0; rowId < mCol; rowId++) {
    for (UINT colId = 0; colId < mRow; colId++) {
      std::printf("%.2f ", dist[rowId * mRow + colId]);
    }
    std::printf("\n");
  }
  std::printf("len of routes = %d\n", routes[0]);
  for (int i = 0; i < routes[0]; i += 2) {
    int startIdx = routes[1 + i];
    int endIdx = routes[2 + i];
    int startX = startIdx % mRow;
    int startY = startIdx / mRow;
    int endX = endIdx % mRow;
    int endY = endIdx / mRow;
    printf("(%d, %d) - (%d, %d)\n", startX, startY, endX, endY);
  }
}

void BasicGamer2DApp::CreateBuffer() {
  // Create DefaultBuffer
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mRow * mCol * sizeof(float),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(&mCostHorizontalBuffer)));
  d3dSetDebugName(mCostHorizontalBuffer.Get(), "CostHorizontal");
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mRow * mCol * sizeof(float),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr,
      IID_PPV_ARGS(&mCostVerticalBuffer)));
  d3dSetDebugName(mCostVerticalBuffer.Get(), "CostVertical");
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mRow * mCol * sizeof(float),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mDistBuffer)));
  d3dSetDebugName(mDistBuffer.Get(), "Dist");
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mMaxTurns * mRow * mCol * sizeof(int),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mAllPrevBuffer)));
  d3dSetDebugName(mAllPrevBuffer.Get(), "AllPrev");
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mRow * mCol * sizeof(int),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mMarkBuffer)));
  d3dSetDebugName(mMarkBuffer.Get(), "Mark");
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          2 * mNumPins * mMaxTurns * sizeof(int),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mRoutesBuffer)));
  d3dSetDebugName(mRoutesBuffer.Get(), "Routes");
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mNumPins * sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mIsRoutedPinBuffer)));
  d3dSetDebugName(mIsRoutedPinBuffer.Get(), "IsRoutedPin");
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mNumPins * sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mPinIndicesBuffer)));
  d3dSetDebugName(mPinIndicesBuffer.Get(), "PinIndices");

  // Create UploadBuffer
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mRow * mCol * sizeof(float)),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mCostHorizontalUploadBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mRow * mCol * sizeof(float)),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mCostVerticalUploadBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mRow * mCol * sizeof(float)),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mMarkUploadBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mNumPins * sizeof(int)),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mPinIndicesUploadBuffer)));

  // Create ReadbackBuffer
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(2 * mNumPins * mMaxTurns * sizeof(int)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&mRoutesReadbackBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mRow * mCol * sizeof(float)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&mDistReadbackBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mMaxTurns * mRow * mCol * sizeof(int)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&mAllPrevReadbackBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(mRow * mCol * sizeof(int)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&mMarkReadbackBuffer)));
}

void BasicGamer2DApp::CreateRootSignature() {
  std::vector<CD3DX12_ROOT_PARAMETER> rootParams;

  // Create RootSignature for SweepHorizontal
  rootParams.resize(4);
  rootParams[0].InitAsConstants(1, 0);
  rootParams[1].InitAsUnorderedAccessView(0);
  rootParams[2].InitAsUnorderedAccessView(1);
  rootParams[3].InitAsUnorderedAccessView(2);
  mSweepHorizontalRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)rootParams.size(), rootParams.data());

  // Create RootSignature for SweepVertical
  rootParams.resize(4);
  rootParams[0].InitAsConstants(1, 0);
  rootParams[1].InitAsUnorderedAccessView(0);
  rootParams[2].InitAsUnorderedAccessView(1);
  rootParams[3].InitAsUnorderedAccessView(2);
  mSweepVerticalRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)rootParams.size(), rootParams.data());

  // Create RootSignature for CleanDist
  rootParams.resize(2);
  rootParams[0].InitAsUnorderedAccessView(0);
  rootParams[1].InitAsUnorderedAccessView(1);
  mCleanDistRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)rootParams.size(), rootParams.data());

  // Create RootSignature for SetRootPin
  rootParams.resize(5);
  rootParams[0].InitAsConstants(1, 0);
  rootParams[1].InitAsUnorderedAccessView(0);
  rootParams[2].InitAsUnorderedAccessView(1);
  rootParams[3].InitAsUnorderedAccessView(2);
  rootParams[4].InitAsUnorderedAccessView(3);
  mSetRootPinRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)rootParams.size(), rootParams.data());

  // Create RootSignature for TracePath
  rootParams.resize(7);
  rootParams[0].InitAsConstants(2, 0);
  rootParams[1].InitAsUnorderedAccessView(0);
  rootParams[2].InitAsUnorderedAccessView(1);
  rootParams[3].InitAsUnorderedAccessView(2);
  rootParams[4].InitAsUnorderedAccessView(3);
  rootParams[5].InitAsUnorderedAccessView(4);
  rootParams[6].InitAsUnorderedAccessView(5);
  mTracePathRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)rootParams.size(), rootParams.data());
}

void BasicGamer2DApp::CreateComputeShader() {
  char rowBuf[16], colBuf[16];
  std::snprintf(rowBuf, 16, "%u", mRow);
  std::snprintf(colBuf, 16, "%u", mCol);
  D3D_SHADER_MACRO macros[] = {
      {"ROW", rowBuf}, {"COL", colBuf}, {nullptr, nullptr}};

  // Create Shader for sweep
  mSweepHorizontalCS =
      d3dUtil::CompileShader(L"SweepHorizontal.hlsl", macros, "main", "cs_5_0");
  mSweepVerticalCS =
      d3dUtil::CompileShader(L"SweepVertical.hlsl", macros, "main", "cs_5_0");

  // Create Shader for CleanDist
  mCleanDistCS =
      d3dUtil::CompileShader(L"CleanDist.hlsl", macros, "main", "cs_5_0");

  // Create Shader for SetRootPin
  mSetRootPinCS =
      d3dUtil::CompileShader(L"SetRootPin.hlsl", macros, "main", "cs_5_0");

  // Create Shader for TracePath
  mTracePathCS =
      d3dUtil::CompileShader(L"TracePath.hlsl", macros, "main", "cs_5_0");
}

void BasicGamer2DApp::CreatePipelineState() {
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};

  // Create PipelineState for SweepHorizontal
  psoDesc.pRootSignature = mSweepHorizontalRootSignature.Get();
  psoDesc.CS = {
      reinterpret_cast<BYTE *>(mSweepHorizontalCS->GetBufferPointer()),
      mSweepHorizontalCS->GetBufferSize()};
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mSweepHorizontalPSO)));

  // Create PipelineState for SweepHorizontal
  psoDesc.pRootSignature = mSweepVerticalRootSignature.Get();
  psoDesc.CS = {reinterpret_cast<BYTE *>(mSweepVerticalCS->GetBufferPointer()),
                mSweepVerticalCS->GetBufferSize()};
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mSweepVerticalPSO)));

  // Create PipelineState for CleanDist
  psoDesc.pRootSignature = mCleanDistRootSignature.Get();
  psoDesc.CS = {reinterpret_cast<BYTE *>(mCleanDistCS->GetBufferPointer()),
                mCleanDistCS->GetBufferSize()};
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mCleanDistPSO)));

  // Create PipelineState for SetRootPin
  psoDesc.pRootSignature = mSetRootPinRootSignature.Get();
  psoDesc.CS = {reinterpret_cast<BYTE *>(mSetRootPinCS->GetBufferPointer()),
                mSetRootPinCS->GetBufferSize()};
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mSetRootPinPSO)));

  // Create PipelineState for TracePath
  psoDesc.pRootSignature = mTracePathRootSignature.Get();
  psoDesc.CS = {reinterpret_cast<BYTE *>(mTracePathCS->GetBufferPointer()),
                mTracePathCS->GetBufferSize()};
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mTracePathPSO)));
}
