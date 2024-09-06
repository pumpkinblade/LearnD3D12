#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
using Microsoft::WRL::ComPtr;

// Link necessary d3d12 libraries.
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class ComputeApp {
public:
  ComputeApp();
  ComputeApp(const ComputeApp &rhs) = delete;
  ComputeApp(ComputeApp &&rhs) = delete;
  ComputeApp &operator=(const ComputeApp &rhs) = delete;
  ComputeApp &operator=(ComputeApp &&rhs) = delete;
  ~ComputeApp() = default;

public:
  static ComputeApp *GetApp();

  void Run();

protected:
  virtual void OnInit();
  virtual void OnCompute();
  virtual void OnDestroy();

  void FlushCommandQueue();
  void LogAdapter(IDXGIAdapter *adapter);

private:
  void CreateDevice();
  void CreateFence();
  void CreateCommandObjects();

protected:
  static ComputeApp *mApp;

  Microsoft::WRL::ComPtr<ID3D12Device> mDevice;

  Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
  UINT64 mFenceValue;
  HANDLE mFenceEvent;

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCmdQueue;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCmdAlloc;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCmdList;

  UINT mCbvSrvUavDescriptorSize;
};
