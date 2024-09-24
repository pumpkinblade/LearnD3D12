#pragma once

#include "Common/d3dApp.h"
#include "Common/Camera.h"

#include "FrameResource.h"
#include "Octree.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;


class RayCastApp : public D3DApp
{
public:
	enum class RenderLayer : int
	{
		Opaque = 0,
		Wireframe,
		OT_Transparent,
		OT_Wireframe,
		Count
	};
public:
	RayCastApp(HINSTANCE hInstance);
	RayCastApp(const RayCastApp& rhs) = delete;
	RayCastApp& operator=(const RayCastApp& rhs) = delete;
	~RayCastApp();

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
	void UpdateGameObjects(const GameTimer& gt);

	void RayCast();
	void OctreeRayCast();

	void LoadTextures();

	void BulidDescriptorHeap();
	void BuildRootSignature();
	void BuildBoxGeometry();
	void BuildMaterials();
	void BuildPSOs();
	void BuildGameObjects();
	void BuildOctree();
	void BuildRenderItems();
	void BuildFrameResources();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<std::unique_ptr<GameObject>> mGameObjects;
	GameObject* mSelectedObject = nullptr;
	GameObject* mOctSelectedObject = nullptr;
	UINT mMark = 0;
	std::unique_ptr<Octree<OctreeData>> mOctree = nullptr;
	XMFLOAT4X4 mOctreeAffine;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(UINT)RenderLayer::Count];

	PassConstants mMainPassCB;

	Camera mCamera;

	POINT mLastMousePos = { 0, 0 };
};
