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

// ����CPUΪ����ÿ֡�����б��������Դ
// ���е�������������죬��ȡ����ʵ�ʻ����������Դ
struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	// ��GPU��������������������ص�����֮ǰ�����ǲ��ܶ�����������
	// ���ÿһ֡��Ҫ���Լ������������
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	// ��GPUִ�������ô˳���������������֮ǰ�����ǲ��ܶ������и���
	// ���ÿһ֡��Ҫ���Լ��ĳ���������
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;

	// ͨ��Χ��ֵ�������ǵ���Χ���㣬��ʹ���ǿ��Լ�⵽GPU�Ƿ���ʹ����Щ��Դ
	UINT64 Fence = 0;
};

// �洢����ͼ������������������ṹ��
struct RenderItem
{
	RenderItem() = default;

	// Model/World����
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 WorldInvTranspose = MathHelper::Identity4x4();
	// Tex
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	XMFLOAT2 DisplacementMapTexelSize;
	float GridSpatialStep;

	// ����������ʾ�������������Ѿ������ı䣬����ζ�Ŵ�ʱ��Ҫ���³�����������
	// ����ÿ��FrameResource�ж���һ��Object�������������������Ǳ����ÿ��
	// FrameResource�����и��¡������������޸��������ݵ�ʱ��Ӧ����
	// NumFramesDirty = gNumFrameResources
	// �Ӷ�ʹÿ��֡��Դ���õ�����
	int NumFramesDirty = gNumFrameResources;

	// ������ָ���GPU������������Ӧ�ڵ�ǰ��Ⱦ���е����峣��������
	UINT ObjCBIndex = -1;

	// ����Ⱦ�������Ƶļ����塣����һ����������ܻ��õ������Ⱦ��
	MeshGeometry* Geo = nullptr;

	// ����Ĳ��ʣ�һ�����ʿ���Ϊ���������ʹ��
	Material* Mat = nullptr;

	// ͼԪ����
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawInstancedInstanced�����Ĳ���
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};
