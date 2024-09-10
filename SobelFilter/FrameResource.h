#pragma once
#include <Common/d3dUtil.h>
#include <Common/UploadBuffer.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

struct PassConstants
{
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;

	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float FogStart = 10.0f;
	float FogRange = 200.0f;
	XMFLOAT2 cbPerPassPad2;

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light Lights[MaxLights];
};

struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 WorldInvTranspose = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	XMFLOAT2 DisplacementMapTexelSize;
	float GridSpatialStep;
	float cbPerObjectPad1;
};

// 存有CPU为构建每帧命令列表所需的资源
// 其中的数据依程序而异，这取决于实际绘制所需的资源
struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	// 在GPU出来完与此命令分配器相关的命令之前，我们不能对它进行重置
	// 因此每一帧都要有自己的命令分配器
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	// 在GPU执行完引用此常量缓冲区的命令之前，我们不能对它进行更新
	// 因此每一帧都要有自己的常量缓冲区
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;

	// 通过围栏值将命令标记到此围栏点，这使我们可以检测到GPU是否还在使用这些资源
	UINT64 Fence = 0;
};

// 存储绘制图形所需参数的轻量级结构体
struct RenderItem
{
	RenderItem() = default;

	// Model/World矩阵
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 WorldInvTranspose = MathHelper::Identity4x4();
	// Tex
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	XMFLOAT2 DisplacementMapTexelSize;
	float GridSpatialStep;

	// 用脏标记来表示物体的相关数据已经发生改变，这意味着此时需要更新常量缓冲区。
	// 由于每个FrameResource中都有一个Object常量缓冲区，所以我们必须对每个
	// FrameResource都进行更新。即，当我们修改物体数据的时候，应该令
	// NumFramesDirty = gNumFrameResources
	// 从而使每个帧资源都得到更新
	int NumFramesDirty = gNumFrameResources;

	// 该索引指向的GPU常量缓冲区对应于当前渲染项中的物体常量缓冲区
	UINT ObjCBIndex = -1;

	// 此渲染项参与绘制的几何体。绘制一个几何体可能会用到多个渲染项
	MeshGeometry* Geo = nullptr;

	// 物体的材质，一个材质可以为多个物体所使用
	Material* Mat = nullptr;

	// 图元拓扑
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawInstancedInstanced方法的参数
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};
