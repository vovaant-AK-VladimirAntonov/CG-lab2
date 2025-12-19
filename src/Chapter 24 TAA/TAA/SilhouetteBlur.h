//***************************************************************************************
// SilhouetteBlur.h - Gaussian blur for static pixels (no velocity)
//***************************************************************************************

#pragma once

#include "../../Common/d3dUtil.h"

class SilhouetteBlur
{
public:
    SilhouetteBlur(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
    
    SilhouetteBlur(const SilhouetteBlur& rhs) = delete;
    SilhouetteBlur& operator=(const SilhouetteBlur& rhs) = delete;
    ~SilhouetteBlur() = default;

    UINT Width() const { return mWidth; }
    UINT Height() const { return mHeight; }
    
    ID3D12Resource* IntermediateResource() { return mIntermediateBuffer.Get(); }
    ID3D12Resource* OutputResource() { return mOutputBuffer.Get(); }
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE IntermediateSrv() const { return mhIntermediateGpuSrv; }
    CD3DX12_CPU_DESCRIPTOR_HANDLE IntermediateRtv() const { return mhIntermediateCpuRtv; }
    
    CD3DX12_GPU_DESCRIPTOR_HANDLE OutputSrv() const { return mhOutputGpuSrv; }
    CD3DX12_CPU_DESCRIPTOR_HANDLE OutputRtv() const { return mhOutputCpuRtv; }

    D3D12_VIEWPORT Viewport() const { return mViewport; }
    D3D12_RECT ScissorRect() const { return mScissorRect; }

    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
        UINT srvDescriptorSize,
        UINT rtvDescriptorSize);

    void OnResize(UINT newWidth, UINT newHeight);

private:
    void BuildDescriptors();
    void BuildResource();

private:
    ID3D12Device* md3dDevice = nullptr;

    D3D12_VIEWPORT mViewport;
    D3D12_RECT mScissorRect;

    UINT mWidth = 0;
    UINT mHeight = 0;
    DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // Intermediate buffer (after horizontal pass)
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntermediateCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhIntermediateGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhIntermediateCpuRtv;
    
    // Output buffer (after vertical pass)
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhOutputCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhOutputGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhOutputCpuRtv;

    Microsoft::WRL::ComPtr<ID3D12Resource> mIntermediateBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> mOutputBuffer = nullptr;
};
