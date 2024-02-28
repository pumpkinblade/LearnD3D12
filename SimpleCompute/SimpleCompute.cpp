#include "SimpleCompute.h"
#include "helper_compute.h"

void SimpleCompute::Init()
{
  UINT dxgiFactoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
  // Enable the D3D12 debug layer
  ComPtr<ID3D12Debug> debugController;
  CHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
  debugController->EnableDebugLayer();
  dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  ComPtr<IDXGIFactory4> factory;
  CHECK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
  // try to create hardware device
  ComPtr<IDXGIAdapter1> hardwareAdapter;
  GetHardwareAdapter(factory.Get(), &hardwareAdapter);
  HRESULT hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
  if (FAILED(hr)) // Fallback to WRAP device
  {
    ComPtr<IDXGIAdapter> warpAdapter;
    CHECK(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    CHECK(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
  }

  // create fence
  CHECK(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
  m_fenceValue = 1;
  // Create an event handle to use for frame synchronization.
  m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (m_fenceEvent == nullptr)
  {
    CHECK(HRESULT_FROM_WIN32(GetLastError()));
  }

  // describe and create the command queue
  D3D12_COMMAND_QUEUE_DESC queueDesc{};
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  CHECK(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_cmdQueue)));
  // create the command allocator
  CHECK(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmdAllocator)));
  // create the command list
  CHECK(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&m_cmdList)));
  // Start off in a closed state.  This is because the first time we refer to
  // the command list we will Reset it, and it needs to be closed before calling Reset.
  CHECK(m_cmdList->Close());

  // create root signature
  // Root parameter can be a table, root descriptor or root constants.
  CD3DX12_ROOT_PARAMETER rootParameters[4];
  rootParameters[0].InitAsConstants(2, 0);
  rootParameters[1].InitAsShaderResourceView(0);
  rootParameters[2].InitAsShaderResourceView(1);
  rootParameters[3].InitAsUnorderedAccessView(0);
  // A root signature is an array of root parameters.
  CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
  rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
  // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
  ComPtr<ID3DBlob> signature = nullptr;
  ComPtr<ID3DBlob> error = nullptr;
  CHECK(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
  CHECK(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

  // shader & pso
  m_shader = LoadBinary("SimpleCompute.cso");
  D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
  computePsoDesc.pRootSignature = m_rootSignature.Get();
  computePsoDesc.CS =
  {
    reinterpret_cast<BYTE*>(m_shader->GetBufferPointer()),
    m_shader->GetBufferSize()
  };
  computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  CHECK(m_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_pipelineState)));
  m_numThreads = 1024;

  m_maxVectorLength = 1024;
  CreateResource();
}

void SimpleCompute::CreateResource()
{
  // create resource
  CHECK(m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(m_maxVectorLength * sizeof(float)),
    D3D12_RESOURCE_STATE_COMMON,
    nullptr,
    IID_PPV_ARGS(m_inputBuffer1.ReleaseAndGetAddressOf())));
  CHECK(m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(m_maxVectorLength * sizeof(float)),
    D3D12_RESOURCE_STATE_COMMON,
    nullptr,
    IID_PPV_ARGS(m_inputBuffer2.ReleaseAndGetAddressOf())));
  CHECK(m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(m_maxVectorLength * sizeof(float), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    nullptr,
    IID_PPV_ARGS(m_outputBuffer.ReleaseAndGetAddressOf())));

  CHECK(m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(m_maxVectorLength * sizeof(float)),
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(m_uploadBuffer1.ReleaseAndGetAddressOf())));
  CHECK(m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(m_maxVectorLength * sizeof(float)),
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(m_uploadBuffer2.ReleaseAndGetAddressOf())));
  CHECK(m_device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(m_maxVectorLength * sizeof(float)),
    D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,
    IID_PPV_ARGS(m_readbackBuffer.ReleaseAndGetAddressOf())));
}

void SimpleCompute::Run(float* out, const float* in1, float const* in2, float alpha, UINT length)
{
  if (length > m_maxVectorLength)
  {
    m_maxVectorLength = std::max(2 * m_maxVectorLength, length);
    CreateResource();
  }

  // copy to gpu
  {
    void* mappedData = nullptr;
    CHECK(m_uploadBuffer1->Map(0, nullptr, &mappedData));
    memcpy(mappedData, in1, length * sizeof(float));
    m_uploadBuffer1->Unmap(0, nullptr);
  }
  {
    void* mappedData = nullptr;
    CHECK(m_uploadBuffer2->Map(0, nullptr, &mappedData));
    memcpy(mappedData, in2, length * sizeof(float));
    m_uploadBuffer2->Unmap(0, nullptr);
  }

  CHECK(m_cmdAllocator->Reset());
  CHECK(m_cmdList->Reset(m_cmdAllocator.Get(), m_pipelineState.Get()));

  // upload
  m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_inputBuffer1.Get(),
    D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
  m_cmdList->CopyResource(m_inputBuffer1.Get(), m_uploadBuffer1.Get());
  m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_inputBuffer1.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
  m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_inputBuffer2.Get(),
    D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST));
  m_cmdList->CopyResource(m_inputBuffer2.Get(), m_uploadBuffer2.Get());
  m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_inputBuffer2.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

  // dispatch
  m_cmdList->SetComputeRootSignature(m_rootSignature.Get());
  m_cmdList->SetComputeRoot32BitConstant(0, *(UINT*)&alpha, 0);
  m_cmdList->SetComputeRoot32BitConstant(0, *(UINT*)&length, 1);
  m_cmdList->SetComputeRootShaderResourceView(1, m_inputBuffer1->GetGPUVirtualAddress());
  m_cmdList->SetComputeRootShaderResourceView(2, m_inputBuffer2->GetGPUVirtualAddress());
  m_cmdList->SetComputeRootUnorderedAccessView(3, m_outputBuffer->GetGPUVirtualAddress());
  m_cmdList->Dispatch((length + m_numThreads - 1) / m_numThreads, 1, 1);

  // download
  m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
    m_outputBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE
  ));
  m_cmdList->CopyResource(m_readbackBuffer.Get(), m_outputBuffer.Get());
  m_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
    m_outputBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  ));

  CHECK(m_cmdList->Close());
  ID3D12CommandList* cmdLists[] = { m_cmdList.Get() };
  m_cmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
  FlushCommandQueue();

  // copy to cpu
  {
    void* mappedData = nullptr;
    CHECK(m_readbackBuffer->Map(0, nullptr, &mappedData));
    memcpy(out, mappedData, length * sizeof(float));
    m_readbackBuffer->Unmap(0, nullptr);
  }
}

void SimpleCompute::Destroy()
{
  // Ensure that the GPU is no longer referencing resources that are about to be
  // cleaned up by the destructor.
  FlushCommandQueue();
  CloseHandle(m_fenceEvent);
}

void SimpleCompute::FlushCommandQueue()
{
  // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
  // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
  // sample illustrates how to use fences for efficient resource usage and to
  // maximize GPU utilization.

  // Signal and increment the fence value.
  const UINT64 fence = m_fenceValue;
  CHECK(m_cmdQueue->Signal(m_fence.Get(), fence));
  m_fenceValue++;

  // Wait until the previous frame is finished.
  if (m_fence->GetCompletedValue() < fence)
  {
    CHECK(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
  }
}