#pragma once

#include "d3dUtil.h"

class ComputeUnit
{
public:
  ComputeUnit();
  ComputeUnit(const ComputeUnit& other) = delete;
  ComputeUnit& operator=(const ComputeUnit& other) = delete;
  ~ComputeUnit();

  virtual bool Initialize();
  virtual int Run();

protected:
  bool InitializeD3D();
  void CreateCommandObjects();
  void FlushCommandQueue();

protected:
  Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
  Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
  Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

  Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
  UINT64 mCurrentFence = 0;

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

  UINT mCbvSrvUavDescriptorSize = 0;
  D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
};