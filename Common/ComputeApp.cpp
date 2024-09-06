//***************************************************************************************
// d3dApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "ComputeApp.h"

using namespace std;
using namespace DirectX;

ComputeApp *ComputeApp::mApp = nullptr;
ComputeApp *ComputeApp::GetApp() { return mApp; }

ComputeApp::ComputeApp() {
  // Only one D3DApp can be constructed.
  assert(mApp == nullptr);
  mApp = this;
}

void ComputeApp::Run() {
  OnInit();
  OnCompute();
  OnDestroy();
}

void ComputeApp::OnInit() {
  CreateDevice();
  CreateFence();
  CreateCommandObjects();
  mCbvSrvUavDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void ComputeApp::OnCompute() {}

void ComputeApp::OnDestroy() {
  FlushCommandQueue();
  CloseHandle(mFenceEvent);
}

void ComputeApp::CreateDevice() {
  UINT dxgiFactoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
  // Enable the D3D12 debug layer
  ComPtr<ID3D12Debug> debugController;
  ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
  debugController->EnableDebugLayer();
  dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  // find one hardware adapter that supports D3D12
  ComPtr<IDXGIFactory4> factory;
  ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
  ComPtr<IDXGIAdapter1> adapter;
  for (UINT adapterIndex = 0;
       factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND;
       ++adapterIndex) {
    // Check to see if the adapter support D3D12, but don't create the actual
    // device yet
    if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    _uuidof(ID3D12Device), nullptr))) {
      break;
    }
    adapter->Release();
  }

  // try to create hardware device
  HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&mDevice));
  if (FAILED(hr)) {
    // Fallback to WRAP device
    ComPtr<IDXGIAdapter> warpAdapter;
    ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&mDevice)));
    LogAdapter(warpAdapter.Get());
  } else {
    LogAdapter(adapter.Get());
  }

  ThrowIfFailed(
      mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
}

void ComputeApp::CreateFence() {
  // create fence
  ThrowIfFailed(
      mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
  mFenceValue = 1;
  // Create an event handle to use for frame synchronization.
  mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (mFenceEvent == nullptr) {
    ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
  }
}

void ComputeApp::CreateCommandObjects() {
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  ThrowIfFailed(
      mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCmdQueue)));

  ThrowIfFailed(mDevice->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(mCmdAlloc.GetAddressOf())));

  ThrowIfFailed(mDevice->CreateCommandList(
      0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
      mCmdAlloc.Get(), // Associated command allocator
      nullptr,         // Initial PipelineStateObject
      IID_PPV_ARGS(mCmdList.GetAddressOf())));

  // Start off in a closed state.  This is because the first time we refer
  // to the command list we will Reset it, and it needs to be closed before
  // calling Reset.
  ThrowIfFailed(mCmdList->Close());
}

void ComputeApp::FlushCommandQueue() {
  // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
  // This is code implemented as such for simplicity. The
  // D3D12HelloFrameBuffering sample illustrates how to use fences for efficient
  // resource usage and to maximize GPU utilization.

  // Signal and increment the fence value.
  const UINT64 fence = mFenceValue;
  ThrowIfFailed(mCmdQueue->Signal(mFence.Get(), fence));
  mFenceValue++;

  // Wait until the previous frame is finished.
  if (mFence->GetCompletedValue() < fence) {
    ThrowIfFailed(mFence->SetEventOnCompletion(fence, mFenceEvent));
    WaitForSingleObject(mFenceEvent, INFINITE);
  }
}

void ComputeApp::LogAdapter(IDXGIAdapter *adapter) {
  DXGI_ADAPTER_DESC desc;
  adapter->GetDesc(&desc);
  std::wstring text = L"***Adapter: ";
  text += desc.Description;
  text += L"\n";
  OutputDebugString(text.c_str());
}
