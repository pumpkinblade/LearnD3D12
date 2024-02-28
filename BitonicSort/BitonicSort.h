#pragma once

#include "helper_compute.h"

class BitonicSort
{
public:
  void Init();
  void Run(int* data, UINT length);
  void Destroy();

private:
  void FlushCommandQueue();
  void CreateResource();

private:
  ComPtr<ID3D12Device> m_device;

  ComPtr<ID3D12Fence> m_fence;
  UINT64 m_fenceValue;
  HANDLE m_fenceEvent;

  ComPtr<ID3D12CommandQueue> m_cmdQueue;
  ComPtr<ID3D12CommandAllocator> m_cmdAllocator;
  ComPtr<ID3D12GraphicsCommandList> m_cmdList;

  ComPtr<ID3D12RootSignature> m_rootSignature;

  ComPtr<ID3DBlob> m_shader;
  ComPtr<ID3D12PipelineState> m_pipelineState;
  UINT m_defaultVectorLength;
  int m_dummyValue;

  ComPtr<ID3D12Resource> m_defaultBuffer;
  ComPtr<ID3D12Resource> m_uploadBuffer;
  ComPtr<ID3D12Resource> m_readbackBuffer;
};
