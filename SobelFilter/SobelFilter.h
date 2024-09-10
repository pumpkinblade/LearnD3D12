#pragma once

#include <Common/d3dUtil.h>

class SobelFilter
{
public:
	///<summary>
	/// The width and height should match the dimensions of the input texture to blur.
	/// Recreate when the screen is resized. 
	///</summary>
	SobelFilter(ID3D12Device* device,
		UINT width, UINT height,
		DXGI_FORMAT format);

	SobelFilter(const SobelFilter& rhs) = delete;
	SobelFilter& operator=(const SobelFilter& rhs) = delete;
	~SobelFilter() = default;

	void Output(
		ID3D12GraphicsCommandList* cmdList,
		ID3D12Resource* output,
		D3D12_RESOURCE_STATES outputState);

	CD3DX12_GPU_DESCRIPTOR_HANDLE EdgeMap();

	static const UINT DescriptorCount() { return 2; }

	void Initialize(ID3D12GraphicsCommandList* cmdList, CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize);

	void OnResize(UINT newWidth, UINT newHeight);

	///<summary>
	/// Blurs the input texture blurCount times.
	///</summary>
	void Execute(
		ID3D12GraphicsCommandList* cmdList,
		CD3DX12_GPU_DESCRIPTOR_HANDLE input);
private:
	void BuildResources();
	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize);
	void BuildDescriptors();
	void BuildRootSignature();
	void BuildPSOs();
private:
	ID3D12Device* md3dDevice = nullptr;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mSobelPSO;
	Microsoft::WRL::ComPtr<ID3DBlob> mSobelCS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mEdgeMapSrvView_CPU;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mEdgeMapUavView_CPU;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mEdgeMapSrvView;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mEdgeMapUavView;

	Microsoft::WRL::ComPtr<ID3D12Resource> mEdgeMap = nullptr;
};
