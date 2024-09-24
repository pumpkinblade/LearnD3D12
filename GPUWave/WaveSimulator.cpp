#include "WaveSimulator.h"

WaveSimulator::WaveSimulator(ID3D12Device* device, int m, int n, float dx, float dt, float speed, float damping)
{
	mNumRows = m;
	mNumCols = n;

	mVertexCount = m * n;
	mTriangleCount = (m - 1) * (n - 1) * 2;

	mTimeStep = dt;
	mSpatialStep = dx;

	float d = damping * dt + 2.0f;
	float e = (speed * speed) * (dt * dt) / (dx * dx);
	mK1 = (damping * dt - 2.0f) / d;
	mK2 = (4.0f - 8.0f * e) / d;
	mK3 = (2.0f * e) / d;

	md3dDevice = device;
}

void WaveSimulator::Update(const GameTimer& gt, ID3D12GraphicsCommandList* cmdList)
{
	static float t = 0.0f;

	t += gt.DeltaTime();

	if (t >= mTimeStep)
	{
		t -= mTimeStep;

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mCurrSln.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		));

		cmdList->SetComputeRootSignature(mRootSignature.Get());

		float constants[] = { mK1, mK2, mK3 };
		cmdList->SetComputeRoot32BitConstants(0, 3, constants, 0);
		cmdList->SetComputeRootDescriptorTable(1, mPrevSlnUavView);
		cmdList->SetComputeRootDescriptorTable(2, mCurrSlnUavView);
		cmdList->SetComputeRootDescriptorTable(3, mNextSlnUavView);

		cmdList->SetPipelineState(mUpdatePSO.Get());
		// TODO: 边界问题
		cmdList->Dispatch(mNumRows / 16, mNumCols / 16, 1);

		// prev -> curr -> next
    mNextSln.Swap(mPrevSln);
    mCurrSln.Swap(mPrevSln);

		auto tempSrvView = mPrevSlnSrvView;
		mPrevSlnSrvView = mCurrSlnSrvView;
		mCurrSlnSrvView = mNextSlnSrvView;
		mNextSlnSrvView = tempSrvView;

		auto tempUavView = mPrevSlnUavView;
		mPrevSlnUavView = mCurrSlnUavView;
		mCurrSlnUavView = mNextSlnUavView;
		mNextSlnUavView = tempUavView;

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			mCurrSln.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ
		));
	}
}

void WaveSimulator::Disturb(ID3D12GraphicsCommandList* cmdList, int i, int j, float magnitude)
{
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mCurrSln.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	));

	cmdList->SetComputeRootSignature(mRootSignature.Get());

	cmdList->SetComputeRoot32BitConstants(0, 1, &magnitude, 3);
	UINT disturbIndex[2] = { j, i };
	cmdList->SetComputeRoot32BitConstants(0, 2, disturbIndex, 4);
	cmdList->SetComputeRootDescriptorTable(3, mCurrSlnUavView);

	cmdList->SetPipelineState(mDisturbPSO.Get());
	cmdList->Dispatch(1, 1, 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mCurrSln.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ
	));
}

CD3DX12_GPU_DESCRIPTOR_HANDLE WaveSimulator::DispatchmentMap()
{
	return mCurrSlnSrvView;
}

void WaveSimulator::Initialze(ID3D12GraphicsCommandList* cmdList, CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize)
{
	BuildResources(cmdList);
	BuildDescriptors(cpuHandle, gpuHandle, descriptorSize);
	BuildRootSignature();
	BuildPSOs();
}

void WaveSimulator::BuildResources(ID3D12GraphicsCommandList* cmdList)
{
	D3D12_RESOURCE_DESC f32Desc;
	ZeroMemory(&f32Desc, sizeof(D3D12_RESOURCE_DESC));
	f32Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	f32Desc.Alignment = 0;
	f32Desc.Width = mNumRows;
	f32Desc.Height = mNumCols;
	f32Desc.DepthOrArraySize = 1;
	f32Desc.MipLevels = 1;
	f32Desc.Format = DXGI_FORMAT_R32_FLOAT;
	f32Desc.SampleDesc.Count = 1;
	f32Desc.SampleDesc.Quality = 0;
	f32Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	f32Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&f32Desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mPrevSln)
	));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&f32Desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mCurrSln)
	));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&f32Desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mNextSln)
	));

	// 初始化所有缓冲为0
	const UINT num2DSubresources = f32Desc.DepthOrArraySize * f32Desc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mCurrSln.Get(), 0, 1);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mUploadBuffer.GetAddressOf())));

	std::vector<float> initData;
	initData.resize(mNumRows * mNumCols);
	for (int i = 0; i < initData.size(); ++i)
		initData[i] = 0.0f;
	initData[mNumRows * mNumCols / 2] = 1.0f;

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData.data();
	subResourceData.RowPitch = mNumCols * sizeof(float);
	subResourceData.SlicePitch = subResourceData.RowPitch * mNumRows;

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mPrevSln.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(cmdList, mPrevSln.Get(), mUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mPrevSln.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrSln.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(cmdList, mCurrSln.Get(), mUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrSln.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mNextSln.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void WaveSimulator::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	md3dDevice->CreateShaderResourceView(mPrevSln.Get(), &srvDesc, cpuHandle);
	md3dDevice->CreateShaderResourceView(mCurrSln.Get(), &srvDesc, cpuHandle.Offset(1, descriptorSize));
	md3dDevice->CreateShaderResourceView(mNextSln.Get(), &srvDesc, cpuHandle.Offset(1, descriptorSize));

	md3dDevice->CreateUnorderedAccessView(mPrevSln.Get(), nullptr, &uavDesc, cpuHandle.Offset(1, descriptorSize));
	md3dDevice->CreateUnorderedAccessView(mCurrSln.Get(), nullptr, &uavDesc, cpuHandle.Offset(1, descriptorSize));
	md3dDevice->CreateUnorderedAccessView(mNextSln.Get(), nullptr, &uavDesc, cpuHandle.Offset(1, descriptorSize));

	mPrevSlnSrvView = gpuHandle;
	mCurrSlnSrvView = gpuHandle.Offset(1, descriptorSize);
	mNextSlnSrvView = gpuHandle.Offset(1, descriptorSize);

	mPrevSlnUavView = gpuHandle.Offset(1, descriptorSize);
	mCurrSlnUavView = gpuHandle.Offset(1, descriptorSize);
	mNextSlnUavView = gpuHandle.Offset(1, descriptorSize);

	// Next Descriptor
	gpuHandle.Offset(1, descriptorSize);
	cpuHandle.Offset(1, descriptorSize);
}

void WaveSimulator::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE uavTable0;
	uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable1;
	uavTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE uavTable2;
	uavTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];
	slotRootParameter[0].InitAsConstants(6, 0);
	slotRootParameter[1].InitAsDescriptorTable(1, &uavTable0);
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable1);
	slotRootParameter[3].InitAsDescriptorTable(1, &uavTable2);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
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

void WaveSimulator::BuildPSOs()
{
	mUpdateCS = d3dUtil::CompileShader(L"WaveSimulator.hlsl", nullptr, "UpdateCS", "cs_5_0");
	mDisturbCS = d3dUtil::CompileShader(L"WaveSimulator.hlsl", nullptr, "DisturbCS", "cs_5_0");

	D3D12_COMPUTE_PIPELINE_STATE_DESC updatePsoDesc = {};
	updatePsoDesc.CS = {
		reinterpret_cast<BYTE*>(mUpdateCS->GetBufferPointer()),
		mUpdateCS->GetBufferSize()
	};
	updatePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	updatePsoDesc.pRootSignature = mRootSignature.Get();
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&updatePsoDesc, IID_PPV_ARGS(&mUpdatePSO)));

	D3D12_COMPUTE_PIPELINE_STATE_DESC disturbPsoDesc = {};
	disturbPsoDesc.CS = {
		reinterpret_cast<BYTE*>(mDisturbCS->GetBufferPointer()),
		mDisturbCS->GetBufferSize()
	};
	disturbPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	disturbPsoDesc.pRootSignature = mRootSignature.Get();
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&disturbPsoDesc, IID_PPV_ARGS(&mDisturbPSO)));
}
