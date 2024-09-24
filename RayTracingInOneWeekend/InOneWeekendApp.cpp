#include "InOneWeekendApp.h"
#include <random>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

void InOneWeekendApp::OnInit() {
  ComputeApp::OnInit();
  InitConstant();
  CreateResource();
  CreateRootSignature();
  CreateComputeShader();
  CreatePipelineState();
}

void InOneWeekendApp::OnCompute() {
  ComputeApp::OnCompute();
  // init random
  {
    std::mt19937 rng{std::random_device{}()};
    UINT *mappedData = nullptr;
    ThrowIfFailed(mRandomUploadBuffer->Map(
        0, nullptr, reinterpret_cast<void **>(&mappedData)));
    for (UINT idx = 0; idx < mImageWidth * mImageHeight; idx++) {
      mappedData[idx] = rng();
    }
    mRandomUploadBuffer->Unmap(0, nullptr);
  }

  // set constantbuffer
  int *imageCBMapped = nullptr;
  ThrowIfFailed(mImageCBUploadBuffer->Map(
      0, nullptr, reinterpret_cast<void **>(&imageCBMapped)));
  {
    int *mapped = nullptr;
    ThrowIfFailed(mCameraCBUploadBuffer->Map(
        0, nullptr, reinterpret_cast<void **>(&mapped)));
    memcpy(mapped, &mCameraCB, sizeof(mCameraCB));
    mCameraCBUploadBuffer->Unmap(0, nullptr);
  }
  {
    int *mapped = nullptr;
    ThrowIfFailed(mObjectCBUploadBuffer->Map(
        0, nullptr, reinterpret_cast<void **>(&mapped)));
    //mObjectCB.num_objs = 4;
    memcpy(mapped, &mObjectCB, sizeof(mObjectCB));
    mCameraCBUploadBuffer->Unmap(0, nullptr);
  }

  // reset cmdList
  ThrowIfFailed(mCmdAlloc->Reset());
  ThrowIfFailed(mCmdList->Reset(mCmdAlloc.Get(), nullptr));

  // upload
  mCmdList->ResourceBarrier(
      1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomBuffer.Get(),
                                               D3D12_RESOURCE_STATE_COMMON,
                                               D3D12_RESOURCE_STATE_COPY_DEST));
  mCmdList->CopyResource(mRandomBuffer.Get(), mRandomUploadBuffer.Get());
  mCmdList->ResourceBarrier(
      1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomBuffer.Get(),
                                               D3D12_RESOURCE_STATE_COPY_DEST,
                                               D3D12_RESOURCE_STATE_COMMON));

  std::vector<CD3DX12_RESOURCE_BARRIER> barriers;
  for (UINT i = 0; i < mNumSamples; i++) {
    // reset image constantbuffer
    mImageCB.image_width = mImageWidth;
    mImageCB.image_height = mImageHeight;
    mImageCB.sample_idx = i;
    mImageCB.num_samples = mNumSamples;
    memcpy(imageCBMapped, &mImageCB, sizeof(mImageCB));

    // generateRay
    barriers.clear();
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mRayBuffer.Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mValidBuffer.Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mRandomBuffer.Get()));
    mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
    mCmdList->SetPipelineState(mGenerateRayPipelineState.Get());
    mCmdList->SetComputeRootSignature(mGenerateRayRootSignature.Get());
    mCmdList->SetComputeRootConstantBufferView(
        0, mImageCBUploadBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootConstantBufferView(
        1, mCameraCBUploadBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        2, mRayBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        3, mValidBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        4, mRandomBuffer->GetGPUVirtualAddress());
    mCmdList->Dispatch((mImageWidth + 15) / 16, (mImageHeight + 15) / 16, 1);

    // foward
    for (UINT d = 0; d < mMaxDepth; d++) {
      barriers.clear();
      barriers.push_back(
          CD3DX12_RESOURCE_BARRIER::UAV(mAllColorBuffers[d].Get()));
      barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mRayBuffer.Get()));
      barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mValidBuffer.Get()));
      barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mRandomBuffer.Get()));
      mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());

      mCmdList->SetPipelineState(mForwardPipelineState.Get());
      mCmdList->SetComputeRootSignature(mForwardRootSignature.Get());
      mCmdList->SetComputeRootConstantBufferView(
          0, mImageCBUploadBuffer->GetGPUVirtualAddress());
      mCmdList->SetComputeRootConstantBufferView(
          1, mObjectCBUploadBuffer->GetGPUVirtualAddress());
      mCmdList->SetComputeRootUnorderedAccessView(
          2, mAllColorBuffers[d]->GetGPUVirtualAddress());
      mCmdList->SetComputeRootUnorderedAccessView(
          3, mRayBuffer->GetGPUVirtualAddress());
      mCmdList->SetComputeRootUnorderedAccessView(
          4, mValidBuffer->GetGPUVirtualAddress());
      mCmdList->SetComputeRootUnorderedAccessView(
          5, mRandomBuffer->GetGPUVirtualAddress());
      mCmdList->Dispatch((mImageWidth + 15) / 16, (mImageHeight + 15) / 16, 1);
    }

    // background
    barriers.clear();
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mValidBuffer.Get()));
    barriers.push_back(
        CD3DX12_RESOURCE_BARRIER::UAV(mAllColorBuffers[mMaxDepth].Get()));
    mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
    mCmdList->SetPipelineState(mBackgroundPipelineState.Get());
    mCmdList->SetComputeRootSignature(mBackgroundRootSignature.Get());
    mCmdList->SetComputeRootConstantBufferView(
        0, mImageCBUploadBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        1, mValidBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        2, mAllColorBuffers[mMaxDepth]->GetGPUVirtualAddress());
    mCmdList->Dispatch((mImageWidth + 15) / 16, (mImageHeight + 15) / 16, 1);

    // backward
    for (UINT d = mMaxDepth; d > 0; d--) {
      barriers.clear();
      barriers.push_back(
          CD3DX12_RESOURCE_BARRIER::UAV(mAllColorBuffers[d].Get()));
      barriers.push_back(
          CD3DX12_RESOURCE_BARRIER::UAV(mAllColorBuffers[d - 1].Get()));
      mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
      mCmdList->SetPipelineState(mBackwardPipelineState.Get());
      mCmdList->SetComputeRootSignature(mBackwardRootSignature.Get());
      mCmdList->SetComputeRootConstantBufferView(
          0, mImageCBUploadBuffer->GetGPUVirtualAddress());
      mCmdList->SetComputeRootUnorderedAccessView(
          1, mAllColorBuffers[d]->GetGPUVirtualAddress());
      mCmdList->SetComputeRootUnorderedAccessView(
          2, mAllColorBuffers[d - 1]->GetGPUVirtualAddress());
      mCmdList->Dispatch((mImageWidth + 15) / 16, (mImageHeight + 15) / 16, 1);
    }

    // blend
    barriers.clear();
    barriers.push_back(
        CD3DX12_RESOURCE_BARRIER::UAV(mAllColorBuffers[0].Get()));
    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(mPixelBuffer.Get()));
    mCmdList->ResourceBarrier((UINT)barriers.size(), barriers.data());
    mCmdList->SetPipelineState(mBlendPipelineState.Get());
    mCmdList->SetComputeRootSignature(mBlendRootSignature.Get());
    mCmdList->SetComputeRootConstantBufferView(
        0, mImageCBUploadBuffer->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        1, mAllColorBuffers[0]->GetGPUVirtualAddress());
    mCmdList->SetComputeRootUnorderedAccessView(
        2, mPixelBuffer->GetGPUVirtualAddress());
    mCmdList->Dispatch((mImageWidth + 15) / 16, (mImageHeight + 15) / 16, 1);
  }

  // download
  mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
                                   mPixelBuffer.Get(),
                                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                   D3D12_RESOURCE_STATE_COPY_SOURCE));
  mCmdList->CopyResource(mPixelReadbackBuffer.Get(), mPixelBuffer.Get());

  // flush cmdList
  ThrowIfFailed(mCmdList->Close());
  ID3D12CommandList *cmdLists[] = {mCmdList.Get()};
  mCmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
  FlushCommandQueue();

  // readback color
  {
    XMFLOAT3 *mappedData = nullptr;
    ThrowIfFailed(mPixelReadbackBuffer->Map(
        0, nullptr, reinterpret_cast<void **>(&mappedData)));
    std::vector<unsigned char> imageData(3 * mImageWidth * mImageHeight);
    for (size_t y = 0; y < mImageHeight; y++) {
      for (size_t x = 0; x < mImageWidth; x++) {
        const auto &c = mappedData[y * mImageWidth + x];
        auto r = static_cast<unsigned char>(256 * max(min(c.x, 0.999f), 0.f));
        auto g = static_cast<unsigned char>(256 * max(min(c.y, 0.999f), 0.f));
        auto b = static_cast<unsigned char>(256 * max(min(c.z, 0.999f), 0.f));
        imageData[3 * (y * mImageWidth + x) + 0] = r;
        imageData[3 * (y * mImageWidth + x) + 1] = g;
        imageData[3 * (y * mImageWidth + x) + 2] = b;
      }
    }
    mPixelReadbackBuffer->Unmap(0, nullptr);
    stbi_flip_vertically_on_write(true);
    stbi_write_bmp("image.bmp", mImageWidth, mImageHeight, 3, imageData.data());
  }
}

void InOneWeekendApp::InitConstant() {
  mImageWidth = 256;
  mImageHeight = 192;
  mMaxDepth = 20;
  mNumSamples = 100;

  // Init ImageCB
  {
    mImageCB.image_width = mImageWidth;
    mImageCB.image_height = mImageHeight;
    mImageCB.num_samples = mNumSamples;
    mImageCB.sample_idx = 0;
  }

  // Init CameraCB
  {
    XMFLOAT3 lookfrom(13, 2, 3);
    XMFLOAT3 lookat(0, 0, 0);
    XMFLOAT3 vup(0, 1, 0);
    float dist_to_focus = 10.f;
    float aperture = 0.1f;
    float aspect_ratio =
        static_cast<float>(mImageWidth) / static_cast<float>(mImageHeight);

    float theta = XMConvertToRadians(20.f);
    float h = std::tan(theta / 2.0f);
    float viewport_height = 2.0f * h;
    float viewport_width = aspect_ratio * viewport_height;

    XMVECTOR lookfrom_v = XMLoadFloat3(&lookfrom);
    XMVECTOR lookat_v = XMLoadFloat3(&lookat);
    XMVECTOR vup_v = XMLoadFloat3(&vup);
    XMVECTOR w_v = XMVector3Normalize(lookfrom_v - lookat_v);
    XMVECTOR u_v = XMVector3Normalize(XMVector3Cross(vup_v, w_v));
    XMVECTOR v_v = XMVector3Cross(w_v, u_v);
    XMVECTOR horizontal_v = dist_to_focus * viewport_width * u_v;
    XMVECTOR vertical_v = dist_to_focus * viewport_height * v_v;
    XMVECTOR lower_left_corner_v = lookfrom_v - 0.5f * horizontal_v -
                                   0.5f * vertical_v - dist_to_focus * w_v;

    XMStoreFloat3(&mCameraCB.origin, lookfrom_v);
    XMStoreFloat3(&mCameraCB.lower_left_corner, lower_left_corner_v);
    XMStoreFloat3(&mCameraCB.horizontal, horizontal_v);
    XMStoreFloat3(&mCameraCB.vertical, vertical_v);
    XMStoreFloat3(&mCameraCB.u, u_v);
    XMStoreFloat3(&mCameraCB.v, v_v);
    XMStoreFloat3(&mCameraCB.w, w_v);
    mCameraCB.lens_radius = aperture * 0.5f;
  }

  // Init ObjectCB
  {
    UINT obj_idx = 0;
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> unf(0.f, 1.f);
    mObjectCB.spheres[obj_idx] = Sphere(XMFLOAT3(0.f, -1000.f, 0.f), 1000.f);
    mObjectCB.materials[obj_idx] = Material1(XMFLOAT3(0.5f, 0.5f, 0.5f));
    obj_idx++;

    mObjectCB.spheres[obj_idx] = Sphere(XMFLOAT3(0.f, 1.f, 0.f), 1.f);
    mObjectCB.materials[obj_idx] = Material1(1.5f);
    obj_idx++;
    mObjectCB.spheres[obj_idx] = Sphere(XMFLOAT3(-4.f, 1.f, 0.f), 1.f);
    mObjectCB.materials[obj_idx] = Material1(XMFLOAT3(0.4f, 0.2f, 0.1f));
    obj_idx++;
    mObjectCB.spheres[obj_idx] = Sphere(XMFLOAT3(4.f, 1.f, 0.f), 1.f);
    mObjectCB.materials[obj_idx] = Material1(XMFLOAT3(0.7f, 0.6f, 0.5f), 0.f);
    obj_idx++;

    for (int a = -5; a < 5; a++) {
      for (int b = -5; b < 5; b++) {
        XMFLOAT3 center(a + 0.9f + unf(rng), 0.2, b + 0.9f + unf(rng));
        float choose_mat = unf(rng);
        mObjectCB.spheres[obj_idx] = Sphere(center, 0.2f);
        if (choose_mat < 0.8f) {
          mObjectCB.materials[obj_idx] =
              Material1(XMFLOAT3(unf(rng), unf(rng), unf(rng)));
        } else if (choose_mat < 0.95f) {
          mObjectCB.materials[obj_idx] =
              Material1(XMFLOAT3(0.5f * unf(rng) + 0.5f, 0.5f * unf(rng) + 0.5f,
                                 0.5f * unf(rng) + 0.5f),
                        0.5f * unf(rng));
        } else {
          mObjectCB.materials[obj_idx] = Material1(1.5f);
        }
        obj_idx++;
      }
    }

    mObjectCB.num_objs = obj_idx;
  }
}

void InOneWeekendApp::CreateResource() {
  // create constantbuffer
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          d3dUtil::CalcConstantBufferByteSize(sizeof(ImageCB))),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mImageCBUploadBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          d3dUtil::CalcConstantBufferByteSize(sizeof(CameraCB))),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mCameraCBUploadBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectCB))),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mObjectCBUploadBuffer)));

  // create colorbuffers
  mAllColorBuffers.resize(mMaxDepth + 1);
  for (UINT i = 0; i < mMaxDepth + 1; i++) {
    ThrowIfFailed(mDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(
            mImageWidth * mImageHeight * sizeof(XMFLOAT3),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&mAllColorBuffers[i])));
  }
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mImageWidth * mImageHeight * sizeof(UINT),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mValidBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mImageWidth * mImageHeight * sizeof(Ray),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mRayBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mImageWidth * mImageHeight * sizeof(UINT),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mRandomBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          mImageWidth * mImageHeight * sizeof(XMFLOAT3),
          D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mPixelBuffer)));

  // create corresponding readback, upload buffer
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          GetRequiredIntermediateSize(mPixelBuffer.Get(), 0, 1)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&mPixelReadbackBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          GetRequiredIntermediateSize(mRandomBuffer.Get(), 0, 1)),
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
      IID_PPV_ARGS(&mRandomUploadBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          GetRequiredIntermediateSize(mRayBuffer.Get(), 0, 1)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&mRayReadbackBuffer)));
  ThrowIfFailed(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          GetRequiredIntermediateSize(mValidBuffer.Get(), 0, 1)),
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
      IID_PPV_ARGS(&mValidReadbackBuffer)));
}

void InOneWeekendApp::CreateRootSignature() {
  std::vector<CD3DX12_ROOT_PARAMETER> params;

  // create rootsignature for generate ray
  params.resize(5);
  params[0].InitAsConstantBufferView(0);
  params[1].InitAsConstantBufferView(1);
  params[2].InitAsUnorderedAccessView(0);
  params[3].InitAsUnorderedAccessView(1);
  params[4].InitAsUnorderedAccessView(2);
  mGenerateRayRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)params.size(), params.data());

  // create rootsignature for forward
  params.resize(6);
  params[0].InitAsConstantBufferView(0);
  params[1].InitAsConstantBufferView(1);
  params[2].InitAsUnorderedAccessView(0);
  params[3].InitAsUnorderedAccessView(1);
  params[4].InitAsUnorderedAccessView(2);
  params[5].InitAsUnorderedAccessView(3);
  mForwardRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)params.size(), params.data());

  // create rootsignature for background
  params.resize(3);
  params[0].InitAsConstantBufferView(0);
  params[1].InitAsUnorderedAccessView(0);
  params[2].InitAsUnorderedAccessView(1);
  mBackgroundRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)params.size(), params.data());

  // create rootsignature for backward
  params.resize(3);
  params[0].InitAsConstantBufferView(0);
  params[1].InitAsUnorderedAccessView(0);
  params[2].InitAsUnorderedAccessView(1);
  mBackwardRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)params.size(), params.data());

  // create rootsignature for blend
  params.resize(3);
  params[0].InitAsConstantBufferView(0);
  params[1].InitAsUnorderedAccessView(0);
  params[2].InitAsUnorderedAccessView(1);
  mBlendRootSignature = d3dUtil::CreateRootSignature(
      mDevice.Get(), (UINT)params.size(), params.data());
}

void InOneWeekendApp::CreateComputeShader() {
  mGenerateRayComputeShader = d3dUtil::LoadBinary(L"generateRay.cso");
  mForwardComputeShader = d3dUtil::LoadBinary(L"forward.cso");
  mBackgroundComputeShader = d3dUtil::LoadBinary(L"background.cso");
  mBackwardComputeShader = d3dUtil::LoadBinary(L"backward.cso");
  mBlendComputeShader = d3dUtil::LoadBinary(L"blend.cso");
}

void InOneWeekendApp::CreatePipelineState() {
  D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};

  // generateRay
  psoDesc.pRootSignature = mGenerateRayRootSignature.Get();
  psoDesc.CS.BytecodeLength = mGenerateRayComputeShader->GetBufferSize();
  psoDesc.CS.pShaderBytecode = mGenerateRayComputeShader->GetBufferPointer();
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mGenerateRayPipelineState)));

  // forward
  psoDesc.pRootSignature = mForwardRootSignature.Get();
  psoDesc.CS.BytecodeLength = mForwardComputeShader->GetBufferSize();
  psoDesc.CS.pShaderBytecode = mForwardComputeShader->GetBufferPointer();
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mForwardPipelineState)));

  // background
  psoDesc.pRootSignature = mBackgroundRootSignature.Get();
  psoDesc.CS.BytecodeLength = mBackgroundComputeShader->GetBufferSize();
  psoDesc.CS.pShaderBytecode = mBackgroundComputeShader->GetBufferPointer();
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mBackgroundPipelineState)));

  // backward
  psoDesc.pRootSignature = mBackwardRootSignature.Get();
  psoDesc.CS.BytecodeLength = mBackwardComputeShader->GetBufferSize();
  psoDesc.CS.pShaderBytecode = mBackwardComputeShader->GetBufferPointer();
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mBackwardPipelineState)));

  // blend
  psoDesc.pRootSignature = mBlendRootSignature.Get();
  psoDesc.CS.BytecodeLength = mBlendComputeShader->GetBufferSize();
  psoDesc.CS.pShaderBytecode = mBlendComputeShader->GetBufferPointer();
  ThrowIfFailed(mDevice->CreateComputePipelineState(
      &psoDesc, IID_PPV_ARGS(&mBlendPipelineState)));
}