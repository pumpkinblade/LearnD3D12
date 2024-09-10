#pragma once

#include <Common/d3dApp.h>

#include "FrameResource.h"

#include "WaveSimulator.h"
#include "SobelFilter.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 TexC;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Waves,
	Count
};

class SobelFilterApp : public D3DApp
{
public:
	SobelFilterApp(HINSTANCE hInstance);
	SobelFilterApp(const SobelFilterApp& rhs) = delete;
	SobelFilterApp& operator=(const SobelFilterApp& rhs) = delete;
	~SobelFilterApp();

	virtual bool Initialize() override;
private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void OnMouseScroll(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);

	void LoadTextures();
	void BuildDescriptorHeap();
	void BuildResources();

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle, UINT descriptorSize);
	
	void BuildShadersAndInputLayout();
	void BuildRootSignature();
	void BuildCompositeRootSignature();
	void BuildPSOs();

	void BuildBoxGeometry();
	void BuildLandGeometry();
	void BuildWavesGeometry();

	void BuildMaterials();
	void BuildRenderItems();
	void BuildFrameResources();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	static std::array<CD3DX12_STATIC_SAMPLER_DESC, 2> GetSamplers();

	float GetHillsHeight(float x, float z) const;
	XMFLOAT3 GetHillsNormal(float x, float z) const;
private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mCompositeRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::unique_ptr<WaveSimulator> mWaveSimulator;
	std::unique_ptr<SobelFilter> mSobelFilter;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mTempSrvView;
	ComPtr<ID3D12Resource> mTempBuffer;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(UINT)RenderLayer::Count];

	PassConstants mMainPassCB;
	RenderItem* mWavesRitem = nullptr;

	bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.2f * XM_PI;
	float mRadius = 150.0f;

	POINT mLastMousePos = { 0, 0 };
};
