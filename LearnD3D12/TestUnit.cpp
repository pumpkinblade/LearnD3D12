#include <iostream>
#include "TestUnit.h"
#include "Random.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

bool TestUnit::Initialize()
{
  if (!ComputeUnit::Initialize())
    return false;

  // Reset the command list to prep for initialization commands.
  ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

  BuildBuffers();
  BuildDescriptorHeap();
  BuildDescriptors();
  BuildRootSignature();
  BuildPSOs();

  // Execute the initialization commands.
  ThrowIfFailed(mCommandList->Close());
  ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
  mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

  // Wait until initialization is complete.
  FlushCommandQueue();
  return true;
}

int TestUnit::Run()
{
  ThrowIfFailed(mDirectCmdListAlloc->Reset());

  ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs["TestUnit"].Get()));

  ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvUavHeap.Get() };
  mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

  mCommandList->SetComputeRootSignature(mRootSignature.Get());

  CD3DX12_GPU_DESCRIPTOR_HANDLE handle(mSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
  mCommandList->SetComputeRootDescriptorTable(0, handle);
  handle.Offset(1, mCbvSrvUavDescriptorSize);
  mCommandList->SetComputeRootDescriptorTable(1, handle);

  mCommandList->Dispatch(1, 1, 1);

  mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
    mOutputBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE
  ));

  mCommandList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get());

  mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
    mOutputBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON
  ));

  ThrowIfFailed(mCommandList->Close());

  ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
  mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

  FlushCommandQueue();

  float* mappedData = nullptr;
  ThrowIfFailed(mReadBackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
  for (int i = 0; i < NumDataElements; ++i)
  {
    std::cout << mappedData[i] << '\n';
  }
  mReadBackBuffer->Unmap(0, nullptr);
  return 0;
}

void TestUnit::BuildDescriptorHeap()
{
  D3D12_DESCRIPTOR_HEAP_DESC srvuavHeapDesc;
  srvuavHeapDesc.NumDescriptors = 2;
  srvuavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvuavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  srvuavHeapDesc.NodeMask = 0;
  ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvuavHeapDesc, IID_PPV_ARGS(&mSrvUavHeap)));
}

void TestUnit::BuildBuffers()
{
  // Generate some data.
  std::array<XMFLOAT3, NumDataElements> dataA;
  for (int i = 0; i < NumDataElements; ++i)
  {
    XMFLOAT3 f3 = { 0.0f, 1.0f, 0.0f };
    XMVECTOR v = XMLoadFloat3(&f3);
    v = (Random::Float() * 9.0f + 1.0f) * XMVector3Normalize(v);
    XMStoreFloat3(&dataA[i], v);
    std::cout << dataA[i].x << " " << dataA[i].y << " " << dataA[i].z << std::endl;
  }
  std::cout << "--------------------------\n";

  UINT64 inputByteSize = dataA.size() * sizeof(XMFLOAT3);
  UINT64 outByteSize = dataA.size() * sizeof(float);

  mInputBuffer = d3dUtil::CreateDefaultBuffer(
    md3dDevice.Get(),
    mCommandList.Get(),
    dataA.data(),
    inputByteSize,
    mInputUploadBuffer);

  // Create the buffer that will be a UAV.
  ThrowIfFailed(md3dDevice->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(outByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    nullptr,
    IID_PPV_ARGS(&mOutputBuffer)));

  ThrowIfFailed(md3dDevice->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(outByteSize),
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(&mReadBackBuffer)));
}

void TestUnit::BuildDescriptors()
{
  // Raw Buffer
  /*
  D3D12_BUFFER_SRV srv;
  srv.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
  srv.FirstElement = 0;
  srv.NumElements = NumDataElements;
  srv.StructureByteStride = 0;
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.Buffer = srv;
  srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
  md3dDevice->CreateShaderResourceView(mInputBuffer.Get(), &srvDesc, handle);

  D3D12_BUFFER_UAV uav;
  uav.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
  uav.FirstElement = 0;
  uav.NumElements = NumDataElements;
  uav.StructureByteStride = 0;
  uav.CounterOffsetInBytes = 0;
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
  uavDesc.Buffer = uav;
  uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

  handle.Offset(1, mCbvSrvUavDescriptorSize);
  md3dDevice->CreateUnorderedAccessView(mOutputBuffer.Get(), nullptr, &uavDesc, handle);
  */

  D3D12_BUFFER_SRV srv;
  srv.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
  srv.FirstElement = 0;
  srv.NumElements = NumDataElements;
  srv.StructureByteStride = sizeof(XMFLOAT3);
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.Buffer = srv;
  srvDesc.Format = DXGI_FORMAT_UNKNOWN;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
  md3dDevice->CreateShaderResourceView(mInputBuffer.Get(), &srvDesc, handle);

  D3D12_BUFFER_UAV uav;
  uav.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
  uav.FirstElement = 0;
  uav.NumElements = NumDataElements;
  uav.StructureByteStride = sizeof(float);
  uav.CounterOffsetInBytes = 0;
  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
  uavDesc.Buffer = uav;
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

  handle.Offset(1, mCbvSrvUavDescriptorSize);
  md3dDevice->CreateUnorderedAccessView(mOutputBuffer.Get(), nullptr, &uavDesc, handle);
}

void TestUnit::BuildRootSignature()
{
  // Root parameter can be a table, root descriptor or root constants.
  CD3DX12_ROOT_PARAMETER slotRootParameter[2];

  // Perfomance TIP: Order from most frequent to least frequent.
  CD3DX12_DESCRIPTOR_RANGE srvTable;
  srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  slotRootParameter[0].InitAsDescriptorTable(1, &srvTable);
  CD3DX12_DESCRIPTOR_RANGE uavTable;
  uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
  slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);

  // A root signature is an array of root parameters.
  CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
    0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_NONE);

  // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
  ComPtr<ID3DBlob> serializedRootSig = nullptr;
  ComPtr<ID3DBlob> errorBlob = nullptr;
  HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
    serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

  if (errorBlob != nullptr)
  {
    ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
  }
  ThrowIfFailed(hr);

  ThrowIfFailed(md3dDevice->CreateRootSignature(
    0,
    serializedRootSig->GetBufferPointer(),
    serializedRootSig->GetBufferSize(),
    IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void TestUnit::BuildPSOs()
{
  mShaders["TestUnitCS"] = d3dUtil::CompileShader(L"TestUnit.hlsl", nullptr, "main", "cs_5_0");
  //mShaders["TestUnitCS"] = d3dUtil::LoadBinary(L"TestUnit.cso");
  D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
  computePsoDesc.pRootSignature = mRootSignature.Get();
  computePsoDesc.CS =
  {
    reinterpret_cast<BYTE*>(mShaders["TestUnitCS"]->GetBufferPointer()),
    mShaders["TestUnitCS"]->GetBufferSize()
  };
  computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["TestUnit"])));
}
