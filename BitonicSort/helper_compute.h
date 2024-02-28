#pragma once

#define NOMINMAX

#include <stdio.h>
#include <fstream>
#include <codecvt>
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

using Microsoft::WRL::ComPtr;

#define CHECK(x)                                     \
{                                                    \
  HRESULT __hr = (x);                                \
  if(FAILED(__hr))                                   \
  {                                                  \
    printErrorMessage(__hr, #x, __FILE__, __LINE__); \
    exit(1);                                         \
  }                                                  \
}

inline void printErrorMessage(HRESULT hr, const char* const func, const char* const file, int const line)
{
  CHAR message[512];
  FormatMessageA(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
    nullptr, hr, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), message, 512, nullptr);
  fprintf(
    stderr, "D3D12 error at %s:%d code=0x%0.8x(%s) \"%s\" \n",
    file, line, hr, message, func);
}

inline void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter)
{
  *ppAdapter = nullptr;
  for (UINT adapterIndex = 0; ; ++adapterIndex)
  {
    IDXGIAdapter1* pAdapter = nullptr;
    if (DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapterIndex, &pAdapter))
    {
      // No more adapters to enumerate.
      break;
    }

    // Check to see if the adapter supports Direct3D 12, but don't create the
    // actual device yet.
    if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
    {
      *ppAdapter = pAdapter;
      return;
    }
    pAdapter->Release();
  }
}

inline ComPtr<ID3DBlob> LoadBinary(const std::string& filename)
{
  std::ifstream fin(filename, std::ios::binary);

  fin.seekg(0, std::ios_base::end);
  std::ifstream::pos_type size = (int)fin.tellg();
  fin.seekg(0, std::ios_base::beg);

  ComPtr<ID3DBlob> blob;
  CHECK(D3DCreateBlob(size, blob.GetAddressOf()));

  fin.read((char*)blob->GetBufferPointer(), size);
  fin.close();

  return blob;
}

inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
  const std::string& filename,
  const D3D_SHADER_MACRO* defines,
  const std::string& entrypoint,
  const std::string& target)
{
  UINT compileFlags = 0;
#if defined(_DEBUG) || defined(DBG)
  compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

  std::wstring wfilename = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(filename);
  ComPtr<ID3DBlob> byteCode = nullptr;
  ComPtr<ID3DBlob> errors = nullptr;
  HRESULT hr = D3DCompileFromFile(
    wfilename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
    entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);
  if (errors != nullptr)
  {
    fprintf(stderr, "%s\n", (char*)errors->GetBufferPointer());
  }
  CHECK(hr);
  return byteCode;
}

inline UINT CalculateConstantBufferByteSize(UINT byteSize)
{
  // Constant buffer size is required to be aligned.
  return (byteSize + (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
}