#pragma once
#include "ComputeUnit.h"

class TestUnit : public ComputeUnit
{
public:
	virtual bool Initialize() override;
	virtual int Run() override;

	void BuildDescriptorHeap();
	void BuildBuffers();
	void BuildDescriptors();
	void BuildRootSignature();
	void BuildPSOs();
private:
	static const int NumDataElements = 32;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvUavHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mInputBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mInputUploadBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mOutputBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mReadBackBuffer = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
};