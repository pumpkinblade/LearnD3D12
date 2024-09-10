#include "SobelFilter.h"

SobelFilter::SobelFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
	md3dDevice = device;
	mWidth = width;
	mHeight = height;
	mFormat = format;
}

void SobelFilter::Output(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* output, D3D12_RESOURCE_STATES outputState)
{
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		output, outputState, D3D12_RESOURCE_STATE_COPY_DEST
	));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mEdgeMap.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE
	));
	cmdList->CopyResource(output, mEdgeMap.Get());
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mEdgeMap.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_GENERIC_READ
	));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		output, D3D12_RESOURCE_STATE_COPY_DEST, outputState
	));
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SobelFilter::EdgeMap()
{
	return mEdgeMapSrvView;
}

void SobelFilter::Initialize(ID3D12GraphicsCommandList* cmdList, CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize)
{
	BuildResources();
	BuildDescriptors(cpuHandle, gpuHandle, descriptorSize);
	BuildRootSignature();
	BuildPSOs();
}

void SobelFilter::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResources();

		// New resource, so we need new descriptors to that resource.
		BuildDescriptors();
	}
}

void SobelFilter::Execute(ID3D12GraphicsCommandList* cmdList, CD3DX12_GPU_DESCRIPTOR_HANDLE input)
{
	cmdList->SetComputeRootSignature(mRootSignature.Get());

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mEdgeMap.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	cmdList->SetPipelineState(mSobelPSO.Get());
	cmdList->SetComputeRootDescriptorTable(0, input);
	cmdList->SetComputeRootDescriptorTable(1, mEdgeMapUavView);

	UINT numGroupX = (INT)ceilf(mWidth / 16.0f);
	UINT numGroupY = (INT)ceilf(mHeight / 16.0f);
	cmdList->Dispatch(numGroupX, numGroupY, 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mEdgeMap.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SobelFilter::BuildResources()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mEdgeMap)));
}

void SobelFilter::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize)
{
	mEdgeMapSrvView_CPU = cpuHandle;
	mEdgeMapUavView_CPU = cpuHandle.Offset(1, descriptorSize);

	mEdgeMapSrvView = gpuHandle;
	mEdgeMapUavView = gpuHandle.Offset(1, descriptorSize);

	// Next Descriptor
	cpuHandle.Offset(1, descriptorSize);
	gpuHandle.Offset(1, descriptorSize);

	BuildDescriptors();
}

void SobelFilter::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = mFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	md3dDevice->CreateShaderResourceView(mEdgeMap.Get(), &srvDesc, mEdgeMapSrvView_CPU);
	md3dDevice->CreateUnorderedAccessView(mEdgeMap.Get(), nullptr, &uavDesc, mEdgeMapUavView_CPU);
}

void SobelFilter::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	slotRootParameter[0].InitAsDescriptorTable(1, &srvTable);
	slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		2, slotRootParameter,
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
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

void SobelFilter::BuildPSOs()
{
	mSobelCS = d3dUtil::CompileShader(L"SobelFilter.hlsl", nullptr, "SobelCS", "cs_5_0");

	D3D12_COMPUTE_PIPELINE_STATE_DESC sobelPsoDesc = {};
	sobelPsoDesc.pRootSignature = mRootSignature.Get();
	sobelPsoDesc.CS = {
		reinterpret_cast<BYTE*>(mSobelCS->GetBufferPointer()),
		mSobelCS->GetBufferSize()
	};
	sobelPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&sobelPsoDesc, IID_PPV_ARGS(&mSobelPSO)));
}
