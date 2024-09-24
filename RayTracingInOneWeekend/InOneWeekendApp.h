#pragma once
#include <Common/ComputeApp.h>
#include <DirectXMath.h>

#define MAX_NUM_OBJS 128
#define MATERIAL_LAMBERTIAN 0
#define MATERIAL_METAL 1
#define MATERIAL_GLASS 2

using namespace DirectX;

struct Material1 {
  int type;
  float ir;
  float fuzz;
  UINT pad0;
  XMFLOAT3 color;
  UINT pad1;
  Material1() = default;
  Material1(const XMFLOAT3 &color) : type(MATERIAL_LAMBERTIAN), color(color) {}
  Material1(const XMFLOAT3 &color, float fuzz)
      : type(MATERIAL_METAL), color(color), fuzz(fuzz) {}
  Material1(float ir) : type(MATERIAL_GLASS), color(1.f, 1.f, 1.f), ir(ir) {}
};

struct Sphere {
  XMFLOAT3 center;
  float radius;
  Sphere() = default;
  Sphere(const XMFLOAT3 &c, float r) : center(c), radius(r) {}
};

struct Ray {
  XMFLOAT3 origin;
  UINT pad0;
  XMFLOAT3 direction;
  UINT pad1;
  Ray() = default;
  Ray(const XMFLOAT3 &o, const XMFLOAT3 &d) : origin(o), direction(d) {}
};

struct ImageCB {
  UINT image_width;
  UINT image_height;
  UINT sample_idx;
  UINT num_samples;
};

struct CameraCB {
  XMFLOAT3 origin;
  UINT pad0;
  XMFLOAT3 lower_left_corner;
  UINT pad1;
  XMFLOAT3 horizontal;
  UINT pad2;
  XMFLOAT3 vertical;
  UINT pad3;
  XMFLOAT3 u;
  UINT pad4;
  XMFLOAT3 v;
  UINT pad5;
  XMFLOAT3 w;
  float lens_radius;
};

struct ObjectCB {
  UINT num_objs;
  UINT pad0[3];
  Sphere spheres[MAX_NUM_OBJS];
  Material1 materials[MAX_NUM_OBJS];
};

class InOneWeekendApp : public ComputeApp {
public:
  void OnInit() override;
  void OnCompute() override;

private:
  void InitConstant();
  void CreateResource();
  void CreateRootSignature();
  void CreateComputeShader();
  void CreatePipelineState();

  UINT mMaxDepth;
  UINT mImageWidth;
  UINT mImageHeight;
  UINT mNumSamples;
  ImageCB mImageCB;
  CameraCB mCameraCB;
  ObjectCB mObjectCB;

  // Buffers
  std::vector<ComPtr<ID3D12Resource>> mAllColorBuffers;
  ComPtr<ID3D12Resource> mValidBuffer;
  ComPtr<ID3D12Resource> mRayBuffer;
  ComPtr<ID3D12Resource> mRandomBuffer;
  ComPtr<ID3D12Resource> mPixelBuffer;
  ComPtr<ID3D12Resource> mRandomUploadBuffer;
  ComPtr<ID3D12Resource> mPixelReadbackBuffer;
  ComPtr<ID3D12Resource> mRayReadbackBuffer;
  ComPtr<ID3D12Resource> mValidReadbackBuffer;

  // ConstantBuffers
  ComPtr<ID3D12Resource> mImageCBUploadBuffer;
  ComPtr<ID3D12Resource> mCameraCBUploadBuffer;
  ComPtr<ID3D12Resource> mObjectCBUploadBuffer;

  // generateRay
  ComPtr<ID3D12RootSignature> mGenerateRayRootSignature;
  ComPtr<ID3DBlob> mGenerateRayComputeShader;
  ComPtr<ID3D12PipelineState> mGenerateRayPipelineState;
  // forward
  ComPtr<ID3D12RootSignature> mForwardRootSignature;
  ComPtr<ID3DBlob> mForwardComputeShader;
  ComPtr<ID3D12PipelineState> mForwardPipelineState;
  // backgroud
  ComPtr<ID3D12RootSignature> mBackgroundRootSignature;
  ComPtr<ID3DBlob> mBackgroundComputeShader;
  ComPtr<ID3D12PipelineState> mBackgroundPipelineState;
  // backward
  ComPtr<ID3D12RootSignature> mBackwardRootSignature;
  ComPtr<ID3DBlob> mBackwardComputeShader;
  ComPtr<ID3D12PipelineState> mBackwardPipelineState;
  // blend
  ComPtr<ID3D12RootSignature> mBlendRootSignature;
  ComPtr<ID3DBlob> mBlendComputeShader;
  ComPtr<ID3D12PipelineState> mBlendPipelineState;
};
