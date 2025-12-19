//***************************************************************************************
// SilhouetteBlur.cpp
//***************************************************************************************

#include "SilhouetteBlur.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

SilhouetteBlur::SilhouetteBlur(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
    md3dDevice = device;
    mWidth = width;
    mHeight = height;
    mFormat = format;

    mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    mScissorRect = { 0, 0, (int)width, (int)height };

    BuildResource();
}

void SilhouetteBlur::BuildDescriptors(
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
    UINT srvDescriptorSize,
    UINT rtvDescriptorSize)
{
    // Intermediate buffer descriptors
    mhIntermediateCpuSrv = hCpuSrv;
    mhIntermediateGpuSrv = hGpuSrv;
    mhIntermediateCpuRtv = hCpuRtv;
    
    // Output buffer descriptors (next slots)
    mhOutputCpuSrv = hCpuSrv.Offset(1, srvDescriptorSize);
    mhOutputGpuSrv = hGpuSrv.Offset(1, srvDescriptorSize);
    mhOutputCpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);

    BuildDescriptors();
}

void SilhouetteBlur::OnResize(UINT newWidth, UINT newHeight)
{
    if ((mWidth != newWidth) || (mHeight != newHeight))
    {
        mWidth = newWidth;
        mHeight = newHeight;

        mViewport = { 0.0f, 0.0f, (float)mWidth, (float)mHeight, 0.0f, 1.0f };
        mScissorRect = { 0, 0, (int)mWidth, (int)mHeight };

        BuildResource();
    }
}

void SilhouetteBlur::BuildDescriptors()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = mFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = mFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    // Intermediate buffer
    md3dDevice->CreateShaderResourceView(mIntermediateBuffer.Get(), &srvDesc, mhIntermediateCpuSrv);
    md3dDevice->CreateRenderTargetView(mIntermediateBuffer.Get(), &rtvDesc, mhIntermediateCpuRtv);
    
    // Output buffer
    md3dDevice->CreateShaderResourceView(mOutputBuffer.Get(), &srvDesc, mhOutputCpuSrv);
    md3dDevice->CreateRenderTargetView(mOutputBuffer.Get(), &rtvDesc, mhOutputCpuRtv);
}

void SilhouetteBlur::BuildResource()
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
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    CD3DX12_CLEAR_VALUE optClear(mFormat, clearColor);

    // Create intermediate buffer
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&mIntermediateBuffer)));

    // Create output buffer
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&mOutputBuffer)));
}
