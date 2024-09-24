#include "RayCastApp.h"

#include <Common/GeometryGenerator.h>
#include <iomanip>
#include <stack>
#include <queue>

const int gNumFrameResources = 3;

RayCastApp::RayCastApp(HINSTANCE hInstance)
	:D3DApp(hInstance)
{
}

RayCastApp::~RayCastApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool RayCastApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// 重置命令列表为执行初始化命令做好准备工作
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	LoadTextures();

	BulidDescriptorHeap();
	BuildRootSignature();
	BuildBoxGeometry();
	BuildMaterials();
	BuildGameObjects();
	BuildOctree();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// 执行初始化命令
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 等待初始化命令的完成
	FlushCommandQueue();

	mCamera.SetPosition(40.0f, 40.0f, -10.0f);

	return true;
}

void RayCastApp::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(MathHelper::Pi * 0.25f, AspectRatio(), 1.0f, 1000.0f);
}

void RayCastApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);
	UpdateGameObjects(gt);

	// 循环往复地获取帧资源循环数组中的元素
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// 如果GPU还没有完成当前帧资源的所有命令，就令CPU等待，直到GPU完成命令的执行并抵达这个围栏点
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
	UpdateMaterialCBs(gt);
}

void RayCastApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// 从呈现状态转换到渲染目标状态
	mCommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	// 清除后台缓冲区和深度缓冲区
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// 指定要渲染的目标缓冲区
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();

	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	{
		mCommandList->SetPipelineState(mPSOs["opaque"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
	}

	{
		mCommandList->SetPipelineState(mPSOs["wireframe"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Wireframe]);
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::OT_Wireframe]);
	}

	{
		mCommandList->SetPipelineState(mPSOs["transparent"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::OT_Transparent]);
	}
	// 从渲染目标状态转换到呈现状态
	mCommandList->ResourceBarrier(1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
		)
	);

	// 完成命令的记录
	ThrowIfFailed(mCommandList->Close());

	// 将命令列表加入到命令队列
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 交换前后台缓冲区
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// 增加围栏值
	mCurrFrameResource->Fence = ++mCurrentFence;

	// 向命令队列添加一条指令，以设置新的围栏点
	// GPU还在执行之前传入的命令，所以GPU不会立即设置新的围栏点，这要等到它处理完Signal()函数之前的所有命令
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void RayCastApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);

	if (btnState == MK_RBUTTON)
	{
		RayCast();
		OctreeRayCast();
	}

}

void RayCastApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void RayCastApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// 每个像素对应1/10度
		float dx = XMConvertToRadians(0.1f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.1f * static_cast<float>(y - mLastMousePos.y));

		mCamera.RotateY(dx);
		mCamera.Pitch(dy);

		mCamera.UpdateViewMatrix();
	}
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void RayCastApp::OnMouseScroll(WPARAM btnState, int x, int y)
{
}

void RayCastApp::OnKeyboardInput(const GameTimer& gt)
{
}

void RayCastApp::UpdateCamera(const GameTimer& gt)
{
	float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(30.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-30.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-30.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(30.0f * dt);

	mCamera.UpdateViewMatrix();
}

void RayCastApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// 只有当对应的常量变化时，才更新常量缓冲区
		// 每个帧资源的渲染项都要改变
		if (e->NumFramesDirty > 0)
		{
			ObjectConstants objConstants;
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(world), world));
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void RayCastApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.9f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.5f, 0.5f, 0.5f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void RayCastApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void RayCastApp::UpdateGameObjects(const GameTimer& gt)
{
}

void RayCastApp::RayCast()
{
	auto proj = mCamera.GetProj4x4f();
	auto view = mCamera.GetView();
	auto view_inv = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	float sx = (float)mLastMousePos.x;
	float sy = (float)mLastMousePos.y;

	float vx = (2.0f * sx / mClientWidth - 1.0f) / proj(0, 0);
	float vy = (-2.0f * sy / mClientHeight + 1.0f) / proj(1, 1);

	XMVECTOR rayOriginV = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR rayDirV = XMVectorSet(vx, vy, 1.0f, 0.0f);

	XMVECTOR rayOriginW = XMVector3TransformCoord(rayOriginV, view_inv);
	XMVECTOR rayDirW = XMVector3TransformNormal(rayDirV, view_inv);

	float near_t = MathHelper::Infinity;
	GameObject* near_obj = nullptr;

	for (auto& box : mGameObjects)
	{
		XMMATRIX world = XMLoadFloat4x4(&box->World);
		XMMATRIX world_inv = XMMatrixInverse(&XMMatrixDeterminant(world), world);
		XMVECTOR rayOriginL = XMVector3TransformCoord(rayOriginW, world_inv);
		XMVECTOR rayDirL = XMVector3TransformNormal(rayDirW, world_inv);
		rayDirL = XMVector3Normalize(rayDirL);

		float t;
		if (box->Bounds.Intersects(rayOriginL, rayDirL, t) && t < near_t)
		{
			near_t = t;
			near_obj = box.get();
		}
	}
	if (mSelectedObject != nullptr)
	{
		mSelectedObject->Ritems[1]->Mat = mMaterials["green"].get();
		mSelectedObject->Ritems[1]->NumFramesDirty = gNumFrameResources;
		mSelectedObject = nullptr;
	}
	if (near_obj != nullptr)
	{
		mSelectedObject = near_obj;
		mSelectedObject->Ritems[1]->Mat = mMaterials["red"].get();
		mSelectedObject->Ritems[1]->NumFramesDirty = gNumFrameResources;
	}
}

void RayCastApp::OctreeRayCast()
{
	mRitemLayer[(UINT)RenderLayer::OT_Transparent].clear();

	auto proj = mCamera.GetProj4x4f();
	auto view = mCamera.GetView();
	auto view_inv = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	float sx = (float)mLastMousePos.x;
	float sy = (float)mLastMousePos.y;
	float vx = (2.0f * sx / mClientWidth - 1.0f) / proj(0, 0);
	float vy = (-2.0f * sy / mClientHeight + 1.0f) / proj(1, 1);
	XMVECTOR rayOriginV = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR rayDirV = XMVectorSet(vx, vy, 1.0f, 0.0f);
	XMVECTOR rayOriginW = XMVector3TransformCoord(rayOriginV, view_inv);
	XMVECTOR rayDirW = XMVector3TransformNormal(rayDirV, view_inv);

	// 第一个cell由DirectXMath决定
	// Octree
	//================================
	// 问题：当定位到一个叶子Cell时，还需要检测父Cell，但是由于总是选择第一个射线击中的物体，
	// 这使得如果实际应该选择的物体在目前的Cell的隔壁，但是却去检查父Cell的物体，导致选择失败
	//================================

	UINT times = 0;
	mMark++;
	XMMATRIX affine = XMLoadFloat4x4(&mOctreeAffine);
	XMVECTOR rayOriginO = XMVector3TransformCoord(rayOriginW, affine);
	XMVECTOR rayDirO = XMVector3TransformNormal(rayDirW, affine);
	rayDirO = XMVector3Normalize(rayDirO);

	XMFLOAT3 p, u;
	XMStoreFloat3(&p, rayOriginO);
	XMStoreFloat3(&u, rayDirO);
	bool might_hit = true;
	uint32_t idx = mOctree->LocatePoint(p);
	if (idx == OT_NIL)
	{
		// 这里可能出现问题
		times++;
		float t;
		BoundingBox bounds({ 0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f });
		if (bounds.Intersects(rayOriginO, rayDirO, t))
		{
			// 第一个p或者curr的确定有问题
			// 关键点：八叉树的范围是[0,1)x[0,1)x[0,1)
			// 如果某个p=(1, 1-\epsilon, 1-\epsilon)则判断p在八叉树外
			XMStoreFloat3(&p, rayOriginO + (t + 10*FLT_EPSILON) * rayDirO);
			idx = mOctree->LocatePoint(p);
		}
		else
			might_hit = false;
	}

	float near_t = MathHelper::Infinity;
	GameObject* near_obj = nullptr;
	if (might_hit)
	{
		times += 1;
		for (uint32_t level = 0; level < mOctree->GetLevels(); level++)
		{
			uint32_t curr = mOctree->LocatePoint(p, level);
			float flag = false;
			while (curr != OT_NIL)
			{
				if(level == 0)
					mRitemLayer[(UINT)RenderLayer::OT_Transparent].push_back(mRitemLayer[(UINT)RenderLayer::OT_Wireframe][curr]);
				auto data = mOctree->ReceiveData(curr).ObjectList;
				for (auto& box : data)
				{
					XMMATRIX world = XMLoadFloat4x4(&box->World);
					XMMATRIX world_inv = XMMatrixInverse(&XMMatrixDeterminant(world), world);
					XMVECTOR rayOriginL = XMVector3TransformCoord(rayOriginW, world_inv);
					XMVECTOR rayDirL = XMVector3TransformNormal(rayDirW, world_inv);
					rayDirL = XMVector3Normalize(rayDirL);

					times++;

					float t;
					if (box->Bounds.Intersects(rayOriginL, rayDirL, t) && t < near_t)
					{
						flag = true;
						near_t = t;
						near_obj = box;
					}
				}
				if(flag)
					break;
				curr = mOctree->RayCastNext(curr, p, u);
			}
		}
	}

	if (mOctSelectedObject != nullptr)
	{
		mOctSelectedObject->Ritems[0]->Mat = mMaterials["white"].get();
		mOctSelectedObject->Ritems[0]->NumFramesDirty = gNumFrameResources;
		mOctSelectedObject = nullptr;
	}
	if (near_obj != nullptr)
	{
		mOctSelectedObject = near_obj;
		mOctSelectedObject->Ritems[0]->Mat = mMaterials["purple"].get();
		mOctSelectedObject->Ritems[0]->NumFramesDirty = gNumFrameResources;
	}
	std::stringstream ss;
	ss << "Number of Times (Octree): " << times << '\n';
	OutputDebugStringA(ss.str().c_str());
}

void RayCastApp::LoadTextures()
{
	auto whiteTex = std::make_unique<Texture>();
	whiteTex->Name = "whiteTex";
	whiteTex->Filename = L"../Textures/white1x1.dds";
	ThrowIfFailed(CreateDDSTextureFromFile12(
		md3dDevice.Get(), mCommandList.Get(),
		whiteTex->Filename.c_str(), whiteTex->Resource, whiteTex->UploadHeap
	));

	mTextures[whiteTex->Name] = std::move(whiteTex);
}

void RayCastApp::BulidDescriptorHeap()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 2;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto whiteTex = mTextures["whiteTex"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	srvDesc.Format = whiteTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(whiteTex.Get(), &srvDesc, hDescriptor);
}

void RayCastApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE tex;
	tex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// 根参数
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	slotRootParameter[0].InitAsDescriptorTable(1, &tex, D3D12_SHADER_VISIBILITY_PIXEL); // Texture
	slotRootParameter[1].InitAsConstantBufferView(0); // ObjecetCB
	slotRootParameter[2].InitAsConstantBufferView(1); // PassCB
	slotRootParameter[3].InitAsConstantBufferView(2); // MaterialCB

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	// 根签名是一个根参数的数组
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		4, slotRootParameter,
		1, &pointWrap,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// 单个寄存器槽来创建一个根签名，该槽位指向一个仅含有单个常量缓冲区的描述符区域
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
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
		IID_PPV_ARGS(mRootSignature.GetAddressOf())
	));
}

void RayCastApp::BuildBoxGeometry()
{
	struct Vertex
	{
		XMFLOAT3 Pos;
		XMFLOAT3 Normal;
		XMFLOAT2 TexC;
	};

	GeometryGenerator geoGen;
	auto box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 0);
	std::vector<uint16_t> indices = box.GetIndices16();
	std::vector<Vertex> vertices;
	vertices.resize(box.Vertices.size());

	for (int i = 0; i < vertices.size(); i++)
	{
		vertices[i].Pos = box.Vertices[i].Position;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)indices.size();
	boxSubmesh.StartIndexLocation = 0;
	boxSubmesh.StartIndexLocation = 0;

	geo->DrawArgs["box"] = boxSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void RayCastApp::BuildMaterials()
{
	auto white = std::make_unique<Material>();
	white->Name = "white";
	white->MatCBIndex = 0;
	white->DiffuseSrvHeapIndex = 0;
	white->DiffuseAlbedo = XMFLOAT4(Colors::White);
	white->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	white->Roughness = 0.1f;

	auto green = std::make_unique<Material>();
	green->Name = "green";
	green->MatCBIndex = 1;
	green->DiffuseSrvHeapIndex = 0;
	green->DiffuseAlbedo = XMFLOAT4(Colors::LightGreen);
	green->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	green->Roughness = 0.1f;

	auto red = std::make_unique<Material>();
	red->Name = "red";
	red->MatCBIndex = 2;
	red->DiffuseSrvHeapIndex = 0;
	red->DiffuseAlbedo = XMFLOAT4(Colors::OrangeRed);
	red->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	red->Roughness = 0.1f;

	auto purple = std::make_unique<Material>();
	purple->Name = "purple";
	purple->MatCBIndex = 3;
	purple->DiffuseSrvHeapIndex = 0;
	purple->DiffuseAlbedo = XMFLOAT4(Colors::Purple);
	purple->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	purple->Roughness = 0.1f;

	auto yellow = std::make_unique<Material>();
	yellow->Name = "yellow";
	yellow->MatCBIndex = 4;
	yellow->DiffuseSrvHeapIndex = 0;
	yellow->DiffuseAlbedo = XMFLOAT4(Colors::Yellow);
	yellow->DiffuseAlbedo.w = 0.2f;
	yellow->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	yellow->Roughness = 0.1f;

	mMaterials[white->Name] = std::move(white);
	mMaterials[green->Name] = std::move(green);
	mMaterials[red->Name] = std::move(red);
	mMaterials[purple->Name] = std::move(purple);
	mMaterials[yellow->Name] = std::move(yellow);
}

void RayCastApp::BuildPSOs()
{
	mShaders["LitVS"] = d3dUtil::CompileShader(L"Lit.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["LitPS"] = d3dUtil::CompileShader(L"Lit.hlsl", nullptr, "PS", "ps_5_0");
	mShaders["UnLitVS"] = d3dUtil::CompileShader(L"UnLit.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["UnLitPS"] = d3dUtil::CompileShader(L"UnLit.hlsl", nullptr, "PS", "ps_5_0");

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["LitVS"]->GetBufferPointer()),
		mShaders["LitVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["LitPS"]->GetBufferPointer()),
		mShaders["LitPS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// 线框模式的PSO
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframePsoDesc = opaquePsoDesc;
	wireframePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	wireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	wireframePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["UnLitVS"]->GetBufferPointer()),
		mShaders["UnLitVS"]->GetBufferSize()
	};
	wireframePsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["UnLitPS"]->GetBufferPointer()),
		mShaders["UnLitPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&wireframePsoDesc, IID_PPV_ARGS(&mPSOs["wireframe"])));

	// 
	// 透明PSO
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;
	transparentPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["UnLitVS"]->GetBufferPointer()),
		mShaders["UnLitVS"]->GetBufferSize()
	};
	transparentPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["UnLitPS"]->GetBufferPointer()),
		mShaders["UnLitPS"]->GetBufferSize()
	};
	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));
}

void RayCastApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; i++)
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
}

void RayCastApp::BuildGameObjects()
{
	for (float x = 20.0f; x < 80.0f; x += 10.0f)
	{
		for (float y = 20.0f; y < 80.0f; y += 10.0f)
		{
			for (float z = 20.0f; z < 80.0f; z += 10.0f)
			{
				std::unique_ptr<GameObject> box = std::make_unique<GameObject>();
				box->Position = { x, y, z };
				box->RotateAxis = { 1.0f, 1.0f, 1.0f };
				box->RotateAngle = MathHelper::RandF() * 2.0f * MathHelper::Pi;
				box->Scale = { 5.0f, 5.0f, 5.0f };

				XMMATRIX translation = XMMatrixTranslation(box->Position.x, box->Position.y, box->Position.z);
				XMMATRIX rotation = XMMatrixRotationAxis(XMVectorSet(box->RotateAxis.x, box->RotateAxis.y, box->RotateAxis.z, 0.0f), box->RotateAngle);
				XMMATRIX scale = XMMatrixScaling(box->Scale.x, box->Scale.y, box->Scale.z);
				XMMATRIX world = XMMatrixMultiply(XMMatrixMultiply(scale, rotation), translation);
				XMStoreFloat4x4(&box->World, world);

				BoundingBox::CreateFromPoints(box->Bounds, XMVectorSet(-0.5f, -0.5f, -0.5f, 0.5f), XMVectorSet(0.5f, 0.5f, 0.5f, 0.5f));
				mGameObjects.push_back(std::move(box));

			}
		}
	}
}

void RayCastApp::BuildOctree()
{
	mOctree = std::make_unique<Octree<OctreeData>>(3);

	//===============
	// 确定变换到八叉树范围的仿射变换
	//===============
	XMMATRIX affine = XMMatrixScaling(100.0f, 100.0f, 100.0f);
	affine = XMMatrixInverse(&XMMatrixDeterminant(affine), affine);
	XMStoreFloat4x4(&mOctreeAffine, affine);

	// 填写八叉树的Data
	uint32_t dataIndex = 0;
	for (auto& box : mGameObjects)
	{
		XMMATRIX world = XMLoadFloat4x4(&box->World);
		BoundingBox boundsW;
		box->Bounds.Transform(boundsW, world);

		XMVECTOR center = XMLoadFloat3(&boundsW.Center);
		XMVECTOR extent = XMLoadFloat3(&boundsW.Extents);
		XMVECTOR v_min = XMVector3TransformCoord(center - extent, affine);
		XMVECTOR v_max = XMVector3TransformCoord(center + extent, affine);

		XMFLOAT3 v_min_3f, v_max_3f;
		XMStoreFloat3(&v_min_3f, v_min);
		XMStoreFloat3(&v_max_3f, v_max);

		auto idx = mOctree->LocateRegion(v_min_3f, v_max_3f);
		if (idx != OT_NIL)
		{
			auto& data = mOctree->ReceiveData(idx);
			data.ObjectList.push_back(box.get());
		}
		else
		{
			OutputDebugStringA("out of range\n");
		}
	}
}

void RayCastApp::BuildRenderItems()
{
	UINT objCBIndex = 0;
	for (auto& box : mGameObjects)
	{
		auto boxRitem = std::make_unique<RenderItem>();
		XMMATRIX world = XMLoadFloat4x4(&box->World);
		XMStoreFloat4x4(&boxRitem->World, world);
		boxRitem->ObjCBIndex = objCBIndex++;
		boxRitem->Geo = mGeometries["boxGeo"].get();
		boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
		boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
		boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
		boxRitem->Mat = mMaterials["white"].get();
		mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());

		auto boxBoundsRitem = std::make_unique<RenderItem>();
		BoundingBox BoundsW;
		box->Bounds.Transform(BoundsW, world);
		XMMATRIX translation = XMMatrixTranslation(BoundsW.Center.x, BoundsW.Center.y, BoundsW.Center.z);
		XMMATRIX scale = XMMatrixScaling(BoundsW.Extents.x * 2.01f, BoundsW.Extents.y * 2.01f, BoundsW.Extents.z * 2.01f);
		world = XMMatrixMultiply(scale, translation);
		XMStoreFloat4x4(&boxBoundsRitem->World, world);
		boxBoundsRitem->ObjCBIndex = objCBIndex++;
		boxBoundsRitem->Geo = mGeometries["boxGeo"].get();
		boxBoundsRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		boxBoundsRitem->IndexCount = boxBoundsRitem->Geo->DrawArgs["box"].IndexCount;
		boxBoundsRitem->StartIndexLocation = boxBoundsRitem->Geo->DrawArgs["box"].StartIndexLocation;
		boxBoundsRitem->BaseVertexLocation = boxBoundsRitem->Geo->DrawArgs["box"].BaseVertexLocation;
		boxBoundsRitem->Mat = mMaterials["green"].get();
		mRitemLayer[(int)RenderLayer::Wireframe].push_back(boxBoundsRitem.get());

		box->Ritems[0] = boxRitem.get();
		box->Ritems[1] = boxBoundsRitem.get();
		mAllRitems.push_back(std::move(boxRitem));
		mAllRitems.push_back(std::move(boxBoundsRitem));
	}


	XMMATRIX affine = XMLoadFloat4x4(&mOctreeAffine);
	XMMATRIX affine_inv = XMMatrixInverse(&XMMatrixDeterminant(affine), affine);
	UINT start = 0;
	UINT end = OT_8EXPSUM(mOctree->m_Levels);
	for (uint32_t i = start; i < end; i++)
	{
		auto oct = std::make_unique<RenderItem>();
		XMFLOAT3 v_min, v_max;
		mOctree->CellToAABB(i, v_min, v_max);
		XMVECTOR center = 0.5f * (XMLoadFloat3(&v_max) + XMLoadFloat3(&v_min));
		XMVECTOR extent = 0.5f * (XMLoadFloat3(&v_max) - XMLoadFloat3(&v_min));

		XMMATRIX world = XMMatrixScalingFromVector(2.0f * extent) * XMMatrixTranslationFromVector(center) * affine_inv;
		XMStoreFloat4x4(&oct->World, world);
		oct->ObjCBIndex = objCBIndex++;
		oct->Geo = mGeometries["boxGeo"].get();
		oct->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		oct->IndexCount = oct->Geo->DrawArgs["box"].IndexCount;
		oct->StartIndexLocation = oct->Geo->DrawArgs["box"].StartIndexLocation;
		oct->BaseVertexLocation = oct->Geo->DrawArgs["box"].BaseVertexLocation;
		oct->Mat = mMaterials["yellow"].get();
		mRitemLayer[(int)RenderLayer::OT_Wireframe].push_back(oct.get());
		mAllRitems.push_back(std::move(oct));
	}
}

void RayCastApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto materialCB = mCurrFrameResource->MaterialCB->Resource();

	// 对于每个渲染项来说...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// 设置纹理
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, tex);

		// 设置常量缓冲区
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();
		objCBAddress += ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = materialCB->GetGPUVirtualAddress();
		matCBAddress += ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		// Draw Call
		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}
