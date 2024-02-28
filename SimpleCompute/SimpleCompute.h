#pragma once

#include "helper_compute.h"

class SimpleCompute
{
public:
  void Init();
  void Run(float* out, const float* in1, float const* in2, float alpha, UINT length);
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
  UINT m_numThreads;

  UINT m_maxVectorLength;
  ComPtr<ID3D12Resource> m_inputBuffer1 = nullptr;
  ComPtr<ID3D12Resource> m_inputBuffer2 = nullptr;
  ComPtr<ID3D12Resource> m_outputBuffer = nullptr;
  ComPtr<ID3D12Resource> m_uploadBuffer1 = nullptr;
  ComPtr<ID3D12Resource> m_uploadBuffer2 = nullptr;
  ComPtr<ID3D12Resource> m_readbackBuffer = nullptr;
};
