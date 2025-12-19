//***************************************************************************************
// FSRUpscaler.cpp - AMD FidelityFX Super Resolution integration
//***************************************************************************************

#include "FSRUpscaler.h"

// FidelityFX API headers
#include "Kits/FidelityFX/api/include/ffx_api.h"
#include "Kits/FidelityFX/api/include/ffx_api_types.h"
#include "Kits/FidelityFX/api/include/dx12/ffx_api_dx12.h"
#include "Kits/FidelityFX/upscalers/include/ffx_upscale.h"

FSRUpscaler::FSRUpscaler()
{
}

FSRUpscaler::~FSRUpscaler()
{
    Destroy();
}

void FSRUpscaler::SetSharpness(float sharpness)
{
    if (sharpness < 0.0f) sharpness = 0.0f;
    if (sharpness > 1.0f) sharpness = 1.0f;
    mSharpness = sharpness;
}

bool FSRUpscaler::Initialize(ID3D12Device* device,
                              UINT displayWidth, UINT displayHeight,
                              QualityMode quality)
{
    if (mInitialized)
        Destroy();

    mDevice = device;
    mDisplayWidth = displayWidth;
    mDisplayHeight = displayHeight;
    mQualityMode = quality;
    
    // Calculate render resolution
    GetRenderResolution(displayWidth, displayHeight, mRenderWidth, mRenderHeight);
    
    CreateContext();
    
    return mInitialized;
}

void FSRUpscaler::Destroy()
{
    DestroyContext();
    mDevice = nullptr;
    mInitialized = false;
}

void FSRUpscaler::CreateContext()
{
    if (mFsrContext != nullptr)
        DestroyContext();

    // Create backend descriptor
    ffxCreateBackendDX12Desc backendDesc = {};
    backendDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
    backendDesc.device = mDevice;

    // Create upscale context descriptor
    // Using same size for render and display since FSR is post-TAA (no actual upscaling)
    ffxCreateContextDescUpscale createDesc = {};
    createDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
    createDesc.header.pNext = &backendDesc.header;
    
    createDesc.maxRenderSize.width = mDisplayWidth;
    createDesc.maxRenderSize.height = mDisplayHeight;
    createDesc.maxUpscaleSize.width = mDisplayWidth;
    createDesc.maxUpscaleSize.height = mDisplayHeight;
    
    // Flags for our setup - used mainly for sharpening post-TAA
    createDesc.flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE |
                       FFX_UPSCALE_ENABLE_DEPTH_INVERTED |
                       FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
    
    createDesc.fpMessage = nullptr;

    ffxReturnCode_t result = ffxCreateContext(&mFsrContext, &createDesc.header, nullptr);
    
    if (result == FFX_API_RETURN_OK)
    {
        mInitialized = true;
        OutputDebugStringA("FSR: Context created successfully (post-TAA mode)\n");
    }
    else
    {
        char msg[128];
        sprintf_s(msg, "FSR: Failed to create context, error code: %d\n", result);
        OutputDebugStringA(msg);
        mInitialized = false;
    }
}

void FSRUpscaler::DestroyContext()
{
    if (mFsrContext != nullptr)
    {
        ffxDestroyContext(&mFsrContext, nullptr);
        mFsrContext = nullptr;
    }
    mJitterIndex = 0;
}

void FSRUpscaler::OnResize(UINT displayWidth, UINT displayHeight)
{
    if (mDisplayWidth == displayWidth && mDisplayHeight == displayHeight)
        return;

    mDisplayWidth = displayWidth;
    mDisplayHeight = displayHeight;
    GetRenderResolution(displayWidth, displayHeight, mRenderWidth, mRenderHeight);
    
    // Recreate context with new resolution
    if (mDevice != nullptr)
    {
        CreateContext();
    }
}

void FSRUpscaler::GetRenderResolution(UINT displayWidth, UINT displayHeight,
                                       UINT& renderWidth, UINT& renderHeight)
{
    float ratio = 1.0f;
    
    switch (mQualityMode)
    {
    case QualityMode::NativeAA:
        ratio = 1.0f;
        break;
    case QualityMode::Quality:
        ratio = 1.5f;
        break;
    case QualityMode::Balanced:
        ratio = 1.7f;
        break;
    case QualityMode::Performance:
        ratio = 2.0f;
        break;
    case QualityMode::UltraPerformance:
        ratio = 3.0f;
        break;
    }
    
    renderWidth = (UINT)(displayWidth / ratio);
    renderHeight = (UINT)(displayHeight / ratio);
    
    // Ensure minimum size
    if (renderWidth < 1) renderWidth = 1;
    if (renderHeight < 1) renderHeight = 1;
}

void FSRUpscaler::SetQualityMode(QualityMode mode)
{
    if (mQualityMode == mode)
        return;
        
    mQualityMode = mode;
    GetRenderResolution(mDisplayWidth, mDisplayHeight, mRenderWidth, mRenderHeight);
    
    // Recreate context with new render resolution
    if (mDevice != nullptr && mInitialized)
    {
        CreateContext();
    }
}

int32_t FSRUpscaler::GetJitterPhaseCount()
{
    if (!mInitialized || mFsrContext == nullptr)
        return 1;

    ffxQueryDescUpscaleGetJitterPhaseCount queryDesc = {};
    queryDesc.header.type = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTERPHASECOUNT;
    queryDesc.renderWidth = mRenderWidth;
    queryDesc.displayWidth = mDisplayWidth;
    
    int32_t phaseCount = 1;
    queryDesc.pOutPhaseCount = &phaseCount;
    
    ffxQuery(&mFsrContext, &queryDesc.header);
    
    return phaseCount;
}

void FSRUpscaler::GetJitterOffset(float& jitterX, float& jitterY)
{
    if (!mInitialized || mFsrContext == nullptr)
    {
        jitterX = 0.0f;
        jitterY = 0.0f;
        return;
    }

    int32_t phaseCount = GetJitterPhaseCount();
    
    ffxQueryDescUpscaleGetJitterOffset queryDesc = {};
    queryDesc.header.type = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETJITTEROFFSET;
    queryDesc.index = mJitterIndex;
    queryDesc.phaseCount = phaseCount;
    queryDesc.pOutX = &mJitterX;
    queryDesc.pOutY = &mJitterY;
    
    ffxQuery(&mFsrContext, &queryDesc.header);
    
    jitterX = mJitterX;
    jitterY = mJitterY;
    
    // Increment for next frame
    mJitterIndex++;
}

void FSRUpscaler::Dispatch(ID3D12GraphicsCommandList* cmdList,
                            ID3D12Resource* colorInput,
                            ID3D12Resource* depthInput,
                            ID3D12Resource* motionVectors,
                            ID3D12Resource* output,
                            float deltaTimeMs,
                            float cameraNear,
                            float cameraFar,
                            float cameraFovY,
                            bool reset)
{
    if (!mInitialized || mFsrContext == nullptr)
    {
        OutputDebugStringA("FSR: Cannot dispatch - not initialized\n");
        return;
    }

    // Build dispatch descriptor
    ffxDispatchDescUpscale dispatchDesc = {};
    dispatchDesc.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
    
    dispatchDesc.commandList = cmdList;
    
    // Input resources
    dispatchDesc.color = ffxApiGetResourceDX12(colorInput, FFX_API_RESOURCE_STATE_COMPUTE_READ);
    dispatchDesc.depth = ffxApiGetResourceDX12(depthInput, FFX_API_RESOURCE_STATE_COMPUTE_READ);
    dispatchDesc.motionVectors = ffxApiGetResourceDX12(motionVectors, FFX_API_RESOURCE_STATE_COMPUTE_READ);
    
    // Output resource
    dispatchDesc.output = ffxApiGetResourceDX12(output, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
    
    // Optional resources (set to null/empty)
    dispatchDesc.exposure.resource = nullptr;
    dispatchDesc.reactive.resource = nullptr;
    dispatchDesc.transparencyAndComposition.resource = nullptr;
    
    // Use same size - FSR will apply RCAS sharpening
    dispatchDesc.renderSize.width = mDisplayWidth;
    dispatchDesc.renderSize.height = mDisplayHeight;
    dispatchDesc.upscaleSize.width = mDisplayWidth;
    dispatchDesc.upscaleSize.height = mDisplayHeight;
    
    // No jitter for post-process mode
    dispatchDesc.jitterOffset.x = 0.0f;
    dispatchDesc.jitterOffset.y = 0.0f;
    
    // Motion vector scale
    dispatchDesc.motionVectorScale.x = (float)mDisplayWidth;
    dispatchDesc.motionVectorScale.y = (float)mDisplayHeight;
    
    // Timing
    dispatchDesc.frameTimeDelta = deltaTimeMs;
    
    // Camera parameters
    dispatchDesc.cameraNear = cameraNear;
    dispatchDesc.cameraFar = cameraFar;
    dispatchDesc.cameraFovAngleVertical = cameraFovY;
    dispatchDesc.viewSpaceToMetersFactor = 1.0f;
    
    // Pre-exposure
    dispatchDesc.preExposure = 1.0f;
    
    // Reset temporal history
    dispatchDesc.reset = reset;
    
    // Sharpening only - main benefit when used post-TAA
    dispatchDesc.enableSharpening = mSharpeningEnabled;
    dispatchDesc.sharpness = mSharpness;
    
    // Flags
    dispatchDesc.flags = 0;

    ffxReturnCode_t result = ffxDispatch(&mFsrContext, &dispatchDesc.header);
    
    if (result != FFX_API_RETURN_OK)
    {
        char msg[128];
        sprintf_s(msg, "FSR: Dispatch failed with error code: %d\n", result);
        OutputDebugStringA(msg);
    }
}
