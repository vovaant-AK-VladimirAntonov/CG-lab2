//***************************************************************************************
// FSRUpscaler.h - AMD FidelityFX Super Resolution integration
//***************************************************************************************

#pragma once

#include "../../Common/d3dUtil.h"
#include <d3d12.h>

// Forward declare FFX types to avoid header issues
typedef void* ffxContext;
typedef uint32_t ffxReturnCode_t;

class FSRUpscaler
{
public:
    enum class QualityMode
    {
        NativeAA = 0,       // 1.0x
        Quality = 1,        // 1.5x
        Balanced = 2,       // 1.7x
        Performance = 3,    // 2.0x
        UltraPerformance = 4 // 3.0x
    };

    FSRUpscaler();
    ~FSRUpscaler();

    FSRUpscaler(const FSRUpscaler&) = delete;
    FSRUpscaler& operator=(const FSRUpscaler&) = delete;

    // Initialize FSR context
    bool Initialize(ID3D12Device* device,
                    UINT displayWidth, UINT displayHeight,
                    QualityMode quality = QualityMode::Quality);
    
    void Destroy();
    
    // Resize handling
    void OnResize(UINT displayWidth, UINT displayHeight);
    
    // Get render resolution based on quality mode
    void GetRenderResolution(UINT displayWidth, UINT displayHeight,
                             UINT& renderWidth, UINT& renderHeight);

    // Get jitter offset for current frame (in pixels, render resolution)
    void GetJitterOffset(float& jitterX, float& jitterY);
    
    // Main upscale dispatch
    void Dispatch(ID3D12GraphicsCommandList* cmdList,
                  ID3D12Resource* colorInput,
                  ID3D12Resource* depthInput,
                  ID3D12Resource* motionVectors,
                  ID3D12Resource* output,
                  float deltaTimeMs,
                  float cameraNear,
                  float cameraFar,
                  float cameraFovY,
                  bool reset = false);

    // Accessors
    UINT GetRenderWidth() const { return mRenderWidth; }
    UINT GetRenderHeight() const { return mRenderHeight; }
    UINT GetDisplayWidth() const { return mDisplayWidth; }
    UINT GetDisplayHeight() const { return mDisplayHeight; }
    QualityMode GetQualityMode() const { return mQualityMode; }
    void SetQualityMode(QualityMode mode);
    bool IsInitialized() const { return mInitialized; }
    
    // Sharpening control
    void SetSharpness(float sharpness);
    float GetSharpness() const { return mSharpness; }
    void SetSharpeningEnabled(bool enabled) { mSharpeningEnabled = enabled; }
    bool IsSharpeningEnabled() const { return mSharpeningEnabled; }

private:
    void CreateContext();
    void DestroyContext();
    int32_t GetJitterPhaseCount();

private:
    ID3D12Device* mDevice = nullptr;
    
    ffxContext mFsrContext = nullptr;
    
    UINT mRenderWidth = 0;
    UINT mRenderHeight = 0;
    UINT mDisplayWidth = 0;
    UINT mDisplayHeight = 0;
    
    QualityMode mQualityMode = QualityMode::Quality;
    
    UINT mJitterIndex = 0;
    float mJitterX = 0.0f;
    float mJitterY = 0.0f;
    
    float mSharpness = 1.0f;  // Maximum sharpness for visible effect
    bool mSharpeningEnabled = true;
    bool mInitialized = false;
};
