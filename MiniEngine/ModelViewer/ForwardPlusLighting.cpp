//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author(s):  Alex Nankervis
//             James Stanard
//

#include "ForwardPlusLighting.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "CommandContext.h"
#include "Camera.h"
#include "BufferManager.h"
#include "../3rdParty/NVLC/nvlc.h"

#include "CompiledShaders/FillLightGridCS_8.h"
#include "CompiledShaders/FillLightGridCS_16.h"
#include "CompiledShaders/FillLightGridCS_24.h"
#include "CompiledShaders/FillLightGridCS_32.h"

using namespace Math;

// must keep in sync with HLSL
struct LightData
{
    float pos[3];
    float radiusSq;
    float color[3];

    uint32_t type;
    float coneDir[3];
    float coneAngles[2];

    float shadowTextureMatrix[16];
};

enum { kMinLightGridDim = 8 };

namespace Lighting
{
    IntVar LightGridDim("Application/Forward+/Light Grid Dim", 16, kMinLightGridDim, 32, 8 );

    RootSignature m_FillLightRootSig;
    ComputePSO m_FillLightGridCS_8;
    ComputePSO m_FillLightGridCS_16;
    ComputePSO m_FillLightGridCS_24;
    ComputePSO m_FillLightGridCS_32;

    LightData m_LightData[MaxLights];
    StructuredBuffer m_LightBuffer;
    ByteAddressBuffer m_LightGrid;

    ByteAddressBuffer m_LightGridBitMask;
    uint32_t m_FirstConeLight;
    uint32_t m_FirstConeShadowedLight;

    enum {shadowDim = 512};
    ColorBuffer m_LightShadowArray;
    ShadowBuffer m_LightShadowTempBuffer;
    Matrix4 m_LightShadowMatrix[MaxLights];

    // <<< NVLC =========================================================================

    enum {MaxLightsPerBin = 64};
    enum {LightTypes = 2};
    NvlcContext m_pNVLC;
    sNvlcInit   m_nvlcInit;
    std::unique_ptr<UserDescriptorHeap> m_heapGpuCbvSrvUav;
    std::unique_ptr<DescriptorAllocator> m_heapCpuRtv;
    std::unique_ptr<DescriptorAllocator> m_heapCpuDsv;
    std::unique_ptr<GpuResource> m_texTileData;
    std::unique_ptr<GpuResource> m_bufLightList;

    enum {MaxOmniLights = 32};
    LightData m_LightDataTemp[MaxLights];
    std::vector<sNvlcOmni> m_vOmnis;
    std::vector<sNvlcSpot> m_vSpots;

    // >>> NVLC =========================================================================

    void InitializeResources(void);
    void CreateRandomLights(const Vector3 minBound, const Vector3 maxBound);
    void FillLightGrid(GraphicsContext& gfxContext, const Camera& camera);
    void Shutdown(void);
}

void Lighting::InitializeResources( void )
{
    m_FillLightRootSig.Reset(3, 0);
    m_FillLightRootSig[0].InitAsConstantBuffer(0);
    m_FillLightRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2);
    m_FillLightRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 2);
    m_FillLightRootSig.Finalize(L"FillLightRS");

    m_FillLightGridCS_8.SetRootSignature(m_FillLightRootSig);
    m_FillLightGridCS_8.SetComputeShader(g_pFillLightGridCS_8, sizeof(g_pFillLightGridCS_8));
    m_FillLightGridCS_8.Finalize();

    m_FillLightGridCS_16.SetRootSignature(m_FillLightRootSig);
    m_FillLightGridCS_16.SetComputeShader(g_pFillLightGridCS_16, sizeof(g_pFillLightGridCS_16));
    m_FillLightGridCS_16.Finalize();

    m_FillLightGridCS_24.SetRootSignature(m_FillLightRootSig);
    m_FillLightGridCS_24.SetComputeShader(g_pFillLightGridCS_24, sizeof(g_pFillLightGridCS_24));
    m_FillLightGridCS_24.Finalize();

    m_FillLightGridCS_32.SetRootSignature(m_FillLightRootSig);
    m_FillLightGridCS_32.SetComputeShader(g_pFillLightGridCS_32, sizeof(g_pFillLightGridCS_32));
    m_FillLightGridCS_32.Finalize();

    // <<< NVLC =========================================================================

    m_nvlcInit.uiMaxLightsPerBin = MaxLightsPerBin;
    // todo: assumes 1920x1080 resolution
    m_nvlcInit.uiViewportWidth = Graphics::g_SceneColorBuffer.GetWidth();
    m_nvlcInit.uiViewportHeight = Graphics::g_SceneColorBuffer.GetHeight();
    m_nvlcInit.api = NVLC_API_D3D12;
    m_nvlcInit.bins = NVLC_BIN_2;
    m_nvlcInit.uiGroupsNum = LightTypes;

    sNvlcDimensions dims;
    NVLC_GetDims(nullptr, &dims);

    m_heapGpuCbvSrvUav = std::make_unique<UserDescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, dims.uiRequiredDescriptors_GPU_CbvSrvUav);
    m_heapGpuCbvSrvUav->Create(L"NVCL_heapGpuCbvSrvUav");
    m_heapGpuCbvSrvUav->Alloc(dims.uiRequiredDescriptors_GPU_CbvSrvUav);
    m_heapCpuRtv = std::make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_heapCpuRtv->Allocate(dims.uiRequiredDescriptors_CPU_Rtv);
    m_heapCpuDsv = std::make_unique<DescriptorAllocator>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_heapCpuDsv->Allocate(dims.uiRequiredDescriptors_CPU_Dsv);

    m_nvlcInit.d3d12.pDevice = Graphics::g_Device;
    m_nvlcInit.d3d12.heap_GPU_CbvSrvUav = { m_heapGpuCbvSrvUav->GetHeapPointer(), 0 };
    m_nvlcInit.d3d12.heap_CPU_Rtv = { m_heapCpuRtv->GetCurrentHeapPointer(), 0 };
    m_nvlcInit.d3d12.heap_CPU_Dsv = { m_heapCpuDsv->GetCurrentHeapPointer(), 0 };

    m_pNVLC = NVLC_Init(&m_nvlcInit);

    if (m_pNVLC) {
        sNvlcShaderResource nvlcTileData = NVLC_GetTexture(m_pNVLC, NVLC_TILE_DATA);
        sNvlcShaderResource nvlcLightList = NVLC_GetTexture(m_pNVLC, NVLC_LIGHT_LIST);

        m_texTileData = std::make_unique<GpuResource>(reinterpret_cast<ID3D12Resource*>(nvlcTileData.d3d12.pResource), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_bufLightList = std::make_unique<GpuResource>(reinterpret_cast<ID3D12Resource*>(nvlcTileData.d3d12.pResource), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    // >>> NVLC =========================================================================
}

void Lighting::CreateRandomLights( const Vector3 minBound, const Vector3 maxBound )
{
    Vector3 posScale = maxBound - minBound;
    Vector3 posBias = minBound;

    // todo: replace this with MT
    srand(12645);
    auto randUint = []() -> uint32_t
    {
        return rand(); // [0, RAND_MAX]
    };
    auto randFloat = [randUint]() -> float
    {
        return randUint() * (1.0f / RAND_MAX); // convert [0, RAND_MAX] to [0, 1]
    };
    auto randVecUniform = [randFloat]() -> Vector3
    {
        return Vector3(randFloat(), randFloat(), randFloat());
    };
    auto randGaussian = [randFloat]() -> float
    {
        // polar box-muller
        static bool gaussianPair = true;
        static float y2;

        if (gaussianPair)
        {
            gaussianPair = false;

            float x1, x2, w;
            do
            {
                x1 = 2 * randFloat() - 1;
                x2 = 2 * randFloat() - 1;
                w = x1 * x1 + x2 * x2;
            } while (w >= 1);

            w = sqrt(-2 * log(w) / w);
            y2 = x2 * w;
            return x1 * w;
        }
        else
        {
            gaussianPair = true;
            return y2;
        }
    };
    auto randVecGaussian = [randGaussian]() -> Vector3
    {
        return Normalize(Vector3(randGaussian(), randGaussian(), randGaussian()));
    };

    const float pi = 3.14159265359f;
    for (uint32_t n = 0; n < MaxLights; n++)
    {
        Vector3 pos = randVecUniform() * posScale + posBias;
        float lightRadius = randFloat() * 800.0f + 200.0f;

        Vector3 color = randVecUniform();
        float colorScale = randFloat() * .3f + .3f;
        color = color * colorScale;

        uint32_t type;
        // force types to match 32-bit boundaries for the BIT_MASK_SORTED case
        if (n < 32 * 1)
            type = 0;
        else if (n < 32 * 3)
            type = 1;
        else
            type = 2;

        Vector3 coneDir = randVecGaussian();
        float coneInner = (randFloat() * .2f + .025f) * pi;
        float coneOuter = coneInner + randFloat() * .1f * pi;

        if (type == 1 || type == 2)
        {
            // emphasize cone lights
            color = color * 5.0f;
        }

        Math::Camera shadowCamera;
        shadowCamera.SetEyeAtUp(pos, pos + coneDir, Vector3(0, 1, 0));
        shadowCamera.SetPerspectiveMatrix(coneOuter * 2, 1.0f, lightRadius * .05f, lightRadius * 1.0f);
        shadowCamera.Update();
        m_LightShadowMatrix[n] = shadowCamera.GetViewProjMatrix();
        Matrix4 shadowTextureMatrix = Matrix4(AffineTransform(Matrix3::MakeScale( 0.5f, -0.5f, 1.0f ), Vector3(0.5f, 0.5f, 0.0f))) * m_LightShadowMatrix[n];

        m_LightData[n].pos[0] = pos.GetX();
        m_LightData[n].pos[1] = pos.GetY();
        m_LightData[n].pos[2] = pos.GetZ();
        m_LightData[n].radiusSq = lightRadius * lightRadius;
        m_LightData[n].color[0] = color.GetX();
        m_LightData[n].color[1] = color.GetY();
        m_LightData[n].color[2] = color.GetZ();
        m_LightData[n].type = type;
        m_LightData[n].coneDir[0] = coneDir.GetX();
        m_LightData[n].coneDir[1] = coneDir.GetY();
        m_LightData[n].coneDir[2] = coneDir.GetZ();
        m_LightData[n].coneAngles[0] = 1.0f / (cos(coneInner) - cos(coneOuter));
        m_LightData[n].coneAngles[1] = cos(coneOuter);
        std::memcpy(m_LightData[n].shadowTextureMatrix, &shadowTextureMatrix, sizeof(shadowTextureMatrix));

        // <<< NVLC =========================================================================

        if (type == 0) {
            sNvlcOmni omni = {};
            omni.vCenter[0] = pos.GetX();
            omni.vCenter[1] = pos.GetY();
            omni.vCenter[2] = pos.GetZ();
            omni.fRadius = lightRadius;
            m_vOmnis.push_back(omni);
        }
        if (type > 0) {
            sNvlcSpot spot = {};
            spot.vCenter[0] = pos.GetX();
            spot.vCenter[1] = pos.GetY();
            spot.vCenter[2] = pos.GetZ();
            spot.fRadius = lightRadius;
            spot.vDirection[0] = coneDir.GetX();
            spot.vDirection[1] = coneDir.GetY();
            spot.vDirection[2] = coneDir.GetZ();
            spot.fCosa0 = cos(coneOuter);
            spot.fBulbRadius = 0.0f;
            m_vSpots.push_back(spot);
        }

        // >>> NVLC =========================================================================

        //*(Matrix4*)(m_LightData[n].shadowTextureMatrix) = shadowTextureMatrix;
    }
    // sort lights by type, needed for efficiency in the BIT_MASK approach
    /*	{
    Matrix4 copyLightShadowMatrix[MaxLights];
    memcpy(copyLightShadowMatrix, m_LightShadowMatrix, sizeof(Matrix4) * MaxLights);
    LightData copyLightData[MaxLights];
    memcpy(copyLightData, m_LightData, sizeof(LightData) * MaxLights);

    uint32_t sortArray[MaxLights];
    for (uint32_t n = 0; n < MaxLights; n++)
    {
    sortArray[n] = n;
    }
    std::sort(sortArray, sortArray + MaxLights,
    [this](const uint32_t &a, const uint32_t &b) -> bool
    {
    return this->m_LightData[a].type < this->m_LightData[b].type;
    });
    for (uint32_t n = 0; n < MaxLights; n++)
    {
    m_LightShadowMatrix[n] = copyLightShadowMatrix[sortArray[n]];
    m_LightData[n] = copyLightData[sortArray[n]];
    }
    }*/
    for (uint32_t n = 0; n < MaxLights; n++)
    {
        if (m_LightData[n].type == 1)
        {
            m_FirstConeLight = n;
            break;
        }
    }
    for (uint32_t n = 0; n < MaxLights; n++)
    {
        if (m_LightData[n].type == 2)
        {
            m_FirstConeShadowedLight = n;
            break;
        }
    }
    m_LightBuffer.Create(L"m_LightBuffer", MaxLights, sizeof(LightData), m_LightData);

    // todo: assumes max resolution of 1920x1080
    uint32_t lightGridCells = Math::DivideByMultiple(1920, kMinLightGridDim) * Math::DivideByMultiple(1080, kMinLightGridDim);
    uint32_t lightGridSizeBytes = lightGridCells * (4 + MaxLights * 4);
    m_LightGrid.Create(L"m_LightGrid", lightGridSizeBytes, 1, nullptr);

    uint32_t lightGridBitMaskSizeBytes = lightGridCells * 4 * 4;
    m_LightGridBitMask.Create(L"m_LightGridBitMask", lightGridBitMaskSizeBytes, 1, nullptr);

    m_LightShadowArray.CreateArray(L"m_LightShadowArray", shadowDim, shadowDim, MaxLights, DXGI_FORMAT_R16_UNORM);
    m_LightShadowTempBuffer.Create(L"m_LightShadowTempBuffer", shadowDim, shadowDim);
}

void Lighting::Shutdown(void)
{
    m_LightBuffer.Destroy();
    m_LightGrid.Destroy();
    m_LightGridBitMask.Destroy();
    m_LightShadowArray.Destroy();
    m_LightShadowTempBuffer.Destroy();

    if (m_pNVLC)
    {
        NVLC_Free(m_pNVLC);
        m_pNVLC = nullptr;
    }
}

void Lighting::FillLightGrid(GraphicsContext& gfxContext, const Camera& camera)
{
    ScopedTimer _prof(L"FillLightGrid", gfxContext);

    // <<< NVLC =========================================================================

    sNvlcPerGroupInputOutput light[LightTypes];

    light[0].pCullingData = m_vOmnis.empty() ? nullptr : m_vOmnis.data();
    light[0].uiLights = (unsigned int)m_vOmnis.size();
    light[0].classifyAs = NVLC_CLASSIFY_AS_OMNI;

    light[1].pCullingData = m_vSpots.empty() ? nullptr : &m_vSpots[0];
    light[1].uiLights = (unsigned int)m_vSpots.size();
    light[1].classifyAs = NVLC_CLASSIFY_AS_SPOT;

    sNvlcPrepare input;
    input.cullingTest = NVLC_PS_TRACED;
    input.fMaxBulbRadius = 0.0f;
    input.fPercentOfAvgLightRange = 0.25f;
    input.pGroup = light;

    Matrix4 mView = camera.GetViewMatrix();
    Matrix4 mProj = camera.GetProjMatrix();

    memcpy(input.mProj4x4, &mProj, sizeof(input.mProj4x4));
    memcpy(input.mView4x4, &mView, sizeof(input.mView4x4));

    input.d3d12.pCommandList = gfxContext.GetCommandList();

    NVLC_Prepare(m_pNVLC, &input);

    unsigned int uiVisibleLight = 0;
    for (unsigned int j = 0; j < LightTypes; j++)
    {
        unsigned int uiReadOffset = light[j].classifyAs * MaxOmniLights;
        unsigned int uiCount = light[j].uiVisibleLights;

        for (unsigned int i = 0; i < uiCount; i++)
        {
            unsigned int remap = light[j].pusRemap[i] + uiReadOffset;

            memcpy(&m_LightDataTemp[uiVisibleLight + i], &m_LightData[remap], sizeof(LightData));
        }

        uiVisibleLight += uiCount;
    }

    // TODO: How to recreate the light buffer?
    //m_LightBuffer.Create(L"m_LightBuffer", uiVisibleLight, sizeof(LightData), m_LightDataTemp);

    // >>> NVLC =========================================================================

    ComputeContext& Context = gfxContext.GetComputeContext();

    Context.SetRootSignature(m_FillLightRootSig);

    switch ((int)LightGridDim)
    {
    case  8: Context.SetPipelineState(m_FillLightGridCS_8 ); break;
    case 16: Context.SetPipelineState(m_FillLightGridCS_16); break;
    case 24: Context.SetPipelineState(m_FillLightGridCS_24); break;
    case 32: Context.SetPipelineState(m_FillLightGridCS_32); break;
    default: ASSERT(false); break;
    }

    ColorBuffer& LinearDepth = Graphics::g_LinearDepth[ Graphics::GetFrameCount() % 2 ];

    Context.TransitionResource(m_LightBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(Graphics::g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(m_LightGrid, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(m_LightGridBitMask, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    Context.SetDynamicDescriptor(1, 0, m_LightBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 1, LinearDepth.GetSRV());
    //Context.SetDynamicDescriptor(1, 1, g_SceneDepthBuffer.GetDepthSRV());
    Context.SetDynamicDescriptor(2, 0, m_LightGrid.GetUAV());
    Context.SetDynamicDescriptor(2, 1, m_LightGridBitMask.GetUAV());

    // todo: assumes 1920x1080 resolution
    uint32_t tileCountX = Math::DivideByMultiple(Graphics::g_SceneColorBuffer.GetWidth(), LightGridDim);
    uint32_t tileCountY = Math::DivideByMultiple(Graphics::g_SceneColorBuffer.GetHeight(), LightGridDim);

    float FarClipDist = camera.GetFarClip();
    float NearClipDist = camera.GetNearClip();
    const float RcpZMagic = NearClipDist / (FarClipDist - NearClipDist);

    struct CSConstants
    {
        uint32_t ViewportWidth, ViewportHeight;
        float InvTileDim;
        float RcpZMagic;
        uint32_t TileCount;
        Matrix4 ViewProjMatrix;
    } csConstants;
    // todo: assumes 1920x1080 resolution
    csConstants.ViewportWidth = Graphics::g_SceneColorBuffer.GetWidth();
    csConstants.ViewportHeight = Graphics::g_SceneColorBuffer.GetHeight();
    csConstants.InvTileDim = 1.0f / LightGridDim;
    csConstants.RcpZMagic = RcpZMagic;
    csConstants.TileCount = tileCountX;
    csConstants.ViewProjMatrix = camera.GetViewProjMatrix();
    Context.SetDynamicConstantBufferView(0, sizeof(CSConstants), &csConstants);

    Context.Dispatch(tileCountX, tileCountY, 1);

    Context.TransitionResource(m_LightGrid, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(m_LightGridBitMask, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
