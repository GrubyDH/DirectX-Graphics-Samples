#pragma once

/*
* Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

/*
Developed by Dmitry Zhdan (dzhdan@nvidia.com)

NVLC: NVIDIA raster-based Light Culling library
VERSION: 1.x

//==========================================================================================================================
INITIALIZATION
//==========================================================================================================================

1. #include "nvlc.h"
2. Call NVLC_Init() to create an instance
3. Call NVLC_Free() and NVLC_Init() every time when screen size changes
4. Call NVLC_Free() to destroy an instance

Remarks:
- NVLC supports deferred contexts on DX11 and separate command lists on DX12.
It means that NVLC_Prepare is completely separated from NVLC_Compute. NVLC_Prepare
uses API for copy-only purposes. On DX12 NVLC_Prepare accepts "copy" or "graphics"
command list, while NVLC_Compute requires command list with "graphics" type.
- NVLC_Compute on DX11 do save / restore state, on DX12 does not. On DX12 NVLC_Compute
changes graphics / compute root signatures, viewports, scissors, IA & OM states.

//==========================================================================================================================
INTEGRATION STEPS
//==========================================================================================================================

1. Call NVLC_Prepare() during prepare frame
2. Call NVLC_Compute() after depth buffer fill is done
3. Modify your lighting shader code using "nvlc.hlsl" include file

//==========================================================================================================================
SHADER CODE (pre-shader)
//==========================================================================================================================

#include "nvlc_defines.h"

#define SHADER_API								NVLC_D3D
#define NVLC_BINS								<a value passed to NVLC_Init>
#define NVLC_SMEM_AVAILABLE						0 - PS, 1 - CS

#undef NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN
#define NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN		<a value passed to NVLC_Init>

#define NVLC_DEBUG_BINS							0/1 (if you want to debug something: 0 - debug light lists, 1 - show bins)

#define NVLC_TEX2D_TILE_DATA					<NVLC_TILE_DATA texture>
#define NVLC_BUF_LIGHT_LIST						<NVLC_LIGHT_LIST buffer>

#include "nvlc.hlsl"

//==========================================================================================================================
SHADER CODE (main)
//==========================================================================================================================

NVLC_PRE_SHADER

NVLC_INIT

#ifdef DEBUG

	NVLC_TILE_INFO

#else
	
	if( !NVLC_IS_EARLY_OUT )
	{
		// light type #1

		NVLC_LOOP_BEGIN;

			// use NVLC_LIGHT_ID to access light data
			// do lighting

		NVLC_LOOP_END;

		// light type #2

		NVLC_LOOP_BEGIN;

			// use NVLC_LIGHT_ID to access light data
			// do lighting

		NVLC_LOOP_END;

		// ...

		if( NVLC_IS_OVERFLOW )
			color = float3(1.0, 0.0, 0.0);
	}

#endif
*/

// IMPORTANT: "#define NVLC_USER_LINKAGE" can be used to avoid library linkage here...

#ifdef PLATFORM_API

	#define NVLC_API					PLATFORM_API

#else

	#define NVLC_API					__declspec(dllimport)

	#ifdef _WIN64
		#define NVLC_PLATFORM_EXT		"_x64"
	#else
		#define NVLC_PLATFORM_EXT		"_x32"
	#endif

	#ifdef _DEBUG
		#define NVLC_DEBUG_EXT			"_d"
	#else
		#define NVLC_DEBUG_EXT			""
	#endif

	#ifndef NVLC_USER_LINKAGE
		#pragma comment(lib, "nvlc" NVLC_PLATFORM_EXT NVLC_DEBUG_EXT ".lib")
	#endif

#endif

#define NVLC_ZERO_CONSTRUCTOR(name)		inline name() { memset(this, 0, sizeof(*this)); }

#pragma pack(push, 4)

#define NVLC_OPTIONAL

typedef void*						NvlcContext;
typedef unsigned __int64			NvlcHandle;

//========================================================================================================================
// NVLC data types (D3D11)
//========================================================================================================================

struct sNvlcInit_D3D11
{
	// ID3D11Device
	void*							pDevice;
};

struct sNvlcPrepare_D3D11
{
	// ID3D11DeviceContext
	void*							pDeviceContext;
};

struct sNvlcCompute_D3D11
{
	// ID3D11DeviceContext
	void*							pDeviceContext;

	// ID3D11ShaderResourceView
	void*							pTexDepthOpaque;

	// ID3D11ShaderResourceView
	// [OPTIONAL] can be NULL for opaque-only lighting
	void*							pTexDepthTransparentFront;

	// ID3D11ShaderResourceView
	// [OPTIONAL] can be NULL for opaque-only lighting
	void*							pTexDepthTransparentBack;
};

struct sNvlcShaderResource_D3D11
{
	// ID3D11ShaderResourceView
	void*							pSRV;

	// DXGI_FORMAT
	unsigned						format;
};

//========================================================================================================================
// NVLC data types (D3D12)
//========================================================================================================================

struct sNvlcDescriptorHeapRange_D3D12
{
	// ID3D12DescriptorHeap
    void*							pDescriptorHeap;

	// The base index that can be used as a start index of the pDescriptorHeap
    unsigned						uiBase;
};

struct sNvlcInit_D3D12
{
	// ID3D12Device
	void*							pDevice;

	// ID3D12CommandQueue
	void*							pCommandQueue;

	// Node mask for m-GPU
	unsigned						uiNodeMask;

	// IMPORTANT: a region in each heap must contain at least "sNvlcDimensions::uiRequiredDescriptorSlots" elements

	// Descriptor heap (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
	sNvlcDescriptorHeapRange_D3D12	heap_GPU_CbvSrvUav;

	// Descriptor heap (D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
	sNvlcDescriptorHeapRange_D3D12	heap_CPU_Rtv;

	// Descriptor heap (D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
	sNvlcDescriptorHeapRange_D3D12	heap_CPU_Dsv;
};

struct sNvlcPrepare_D3D12
{
	// ID3D12GraphicsCommandList, type - D3D12_COMMAND_LIST_TYPE_DIRECT or D3D12_COMMAND_LIST_TYPE_COPY
	void*							pCommandList;
};

struct sNvlcCompute_D3D12
{
	// ID3D12Resource, required state - D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	void*							pTexDepthOpaque;

	// ID3D12Resource, required state - D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	// [OPTIONAL] can be NULL for opaque-only lighting
	void*							pTexDepthTransparentFront;

	// ID3D12Resource, required state - D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	// [OPTIONAL] can be NULL for opaque-only lighting
	void*							pTexDepthTransparentBack;

	// ID3D12GraphicsCommandList, type - D3D12_COMMAND_LIST_TYPE_DIRECT
	void*							pCommandList;
};

struct sNvlcShaderResource_D3D12
{
	// ID3D12Resource
	void*							pResource;

	// DXGI_FORMAT
	unsigned						format;

	// Descriptor = heap_GPU_CbvSrvUav.pDescriptorHeap[uiIndex]
	unsigned						uiDescriptorIndex;
};

//========================================================================================================================
// NVLC data types (OGL)
//========================================================================================================================

struct sNvlcShaderResource_OGL
{
	unsigned						glTextureID;
};

struct sNvlcCompute_OGL
{
	// OpenGL texture ID
	unsigned						pTexDepthOpaque;

	// OpenGL texture ID
	// [OPTIONAL] can be 0 for opaque-only lighting
	unsigned						pTexDepthTransparent;
};

//========================================================================================================================
// NVLC data types
//========================================================================================================================

// IMPORTANT: matrices:
// Logic:
//
//      [ Xx Xy Xz Tx ] (Vx)
// V' = [ Yx Yy Yz Ty ] (Vy)
//      [ Zx Zy Zz Tz ] (Vz)
//      [  0  0  0  1 ] ( 1)
//
// Layout:
//
// float m[16] =
// {
//     col0.x, col0.y, col0.z, col0.w
//     col1.x, col1.y, col1.z, col1.w
//     col2.x, col2.y, col2.z, col2.w
//     col3.x, col3.y, col3.z, col3.w
// }

enum eNvlcAPI
{
	NVLC_API_D3D11,
	NVLC_API_D3D12,
	NVLC_API_GL,
	NVLC_API_VK,
};

enum eNvlcClassifyAs
{
	// See declaration of "sNvlcOmni"
	NVLC_CLASSIFY_AS_OMNI,

	// See declaration of "sNvlcSpot"
	NVLC_CLASSIFY_AS_SPOT,

	// See declaration of "sNvlcBox"
	NVLC_CLASSIFY_AS_BOX,

	NVLC_CLASSIFY_AS_NUM
};

enum eNvlcBins
{
	NVLC_BIN_1,
	NVLC_BIN_2,
	NVLC_BIN_4,
	NVLC_BIN_8,
};

enum eNvlcTextureName
{
	// 2D texture (RGBA32ui)
	NVLC_TILE_DATA,

	// Buffer (R16ui)
	NVLC_LIGHT_LIST,
};

enum eNvlcCullingTest
{
	// 1. Ray-primitive test using the advantage of the rasterizer

	NVLC_PS_TRACED,

	// IMPORTANT: for experiments only!
	// - CS-based variants
	// - much slower
	// - some of them are less accurate...
	// - the result is broadcast to all bins (to compare apples-to-apples switching to 1 bin is recommended)

	// 2. Classic frustum-sphere test
	// OMNI - "tile boundary sphere - frustum"
	// SPOT - "cone boundary sphere (accurate) - frustum" + "is tile inside cone"
	// BOX - "tile boundary sphere - aabb"
	
	NVLC_CS_FRUSTUM,

	// 3. Rounded AABB test (Arvo AABB/sphere test)
	// OMNI - "sphere vs tile rounded aabb"
	// SPOT - "sphere vs tile rounded aabb" + "is tile inside cone"
	// BOX - "tile sphere vs box rounded aabb"

	// IMPORTANT: Gareth Tomas from AMD suggests using only Arvo test for spot lights, it is a bad idea and leads to
	// tons of false positives. Removing an additional test (is tile inside cone) makes frustum and rounded aabb tests
	// completely useless.
	// http://www.gdcvault.com/play/1021764/Advanced-Visual-Effects-With-DirectX

	NVLC_CS_ROUNDED_AABB,

	// 4. Ray-primitive test (produces the same results as NVLC_PS_TRACED)
	
	NVLC_CS_TRACED,

	// 5. Test each pixel in a tile against a light volume (reference, very slow)
	// IMPORTANT: transparent layer is ingored
	
	NVLC_CS_REF
};

union sNvlcShaderResource
{
	sNvlcShaderResource_D3D11		d3d11;
	sNvlcShaderResource_D3D12		d3d12;
	sNvlcShaderResource_OGL			ogl;

	NVLC_ZERO_CONSTRUCTOR(sNvlcShaderResource);
};

// Coordinates in world space, radiuses in world units

struct sNvlcOmni
{
	float							vCenter[3];
	float							fRadius;
};

struct sNvlcSpot
{
	float							vCenter[3];
	float							fRadius;
	float							vDirection[3];
	float							fCosa0;
	float							fBulbRadius;
};

struct sNvlcBox
{
	// Defines AABB to world space transformation
	// IMPORTANT: Z = cross(X, Y)
	float							vAxisX[3];
	float							vAxisY[3];
	float							vOrigin[3];

	// Assuming vMin = float3(0, 0, 0)
	float							vMax[3];
};

struct sNvlcPerGroupInputOutput
{
	// INPUT

	// Lights in format described by "classifyAs" (sNvlcOmniGeometry or sNvlcSpotGeometry)
	const void*						pCullingData;

	// Number of entries
	unsigned						uiLights;

	eNvlcClassifyAs					classifyAs;

	// OUTPUT

	// IMPORTANT: don't forget to reorganize your light data!

	const unsigned short*			pusRemap;
	unsigned						uiVisibleLights;

	NVLC_ZERO_CONSTRUCTOR(sNvlcPerGroupInputOutput);
};

struct sNvlcInit
{
	union
	{
		sNvlcInit_D3D11				d3d11;
		sNvlcInit_D3D12				d3d12;
	};

	unsigned						uiViewportWidth;
	unsigned						uiViewportHeight;

	// Number of light groups (types) which will be passed to NVLC_Prepare
	// IMPORTANT: even if group is empty an empty group should be passed!
	unsigned						uiGroupsNum;

	// Maximum lights per bin (worst case)
	// IMPORTANT: includes "end of group" mark for each light group, for example: up to 32 lights from 4 groups = 32 + 4 = 36 lights per bin
	unsigned						uiMaxLightsPerBin;

	eNvlcAPI						api;
	eNvlcBins						bins;

	NVLC_ZERO_CONSTRUCTOR(sNvlcInit);
};

struct sNvlcPrepare
{
	union
	{
		sNvlcPrepare_D3D11			d3d11;
		sNvlcPrepare_D3D12			d3d12;
	};

	// World to view transform
	float							mView4x4[16];

	// View to clip transform - all variant are supported(left/right handed, direct/reversed depth, OGL/D3D style)
	float							mProj4x4[16];

	// Max possible bulb radius
	float							fMaxBulbRadius;

	// Tile will be subdivided until tileZ is larger than this (0.25-0.5 is a good start) (normalized to [0; 1])
	float							fPercentOfAvgLightRange;

	// Light groups (types) used for shading in order of appearence in the shader
	sNvlcPerGroupInputOutput*		pGroup;

	eNvlcCullingTest				cullingTest;

	NVLC_ZERO_CONSTRUCTOR(sNvlcPrepare);
};

union sNvlcCompute
{
	// IMPORTANT:
	// - opaque & transparent depth should have same dimensions
	// - opaque depth can be multisampled if required
	// - transparent depth (back/front) should be non-multisampled
	// - to get transparent front faces depth
	//		clear depth to "far" plane or optionally copy depth from depth prepass
	//		enable depth test
	//		enable depth write
	//		use direct depth test (back to front)
	//		render front faces
	// - to get transparent back faces depth
	//		clear depth to "near" plane
	//		enable depth test
	//		enable depth write
	//		use reversed depth test (front to back)
	//		render back faces

	sNvlcCompute_D3D11				d3d11;
	sNvlcCompute_D3D12				d3d12;
	sNvlcCompute_OGL				ogl;

	NVLC_ZERO_CONSTRUCTOR(sNvlcCompute);
};

struct sNvlcDimensions
{
	unsigned						uiRequiredDescriptors_GPU_CbvSrvUav;
	unsigned						uiRequiredDescriptors_CPU_Rtv;
	unsigned						uiRequiredDescriptors_CPU_Dsv;

	unsigned						uiTileWidth;
	unsigned						uiTileHeight;

	// IMPORTANT: a valid NvlcContext is required to retrieve
	
	unsigned						uiGridWidth;
	unsigned						uiGridHeight;

	NVLC_ZERO_CONSTRUCTOR(sNvlcDimensions);
};

//========================================================================================================================
// API
//========================================================================================================================

// Library allocates 4.67 bytes per light, to get total allocated memory this formula can be used:
// Mem in bytes = 4.67 * [bins] * [lights per bin] * [tiles.x * tiles.y]
// Examples:
//    1920x1080, 2 bins, 64 lights = 4.6 Mb
//    1920x1080, 4 bins, 64 lights = 9.3 Mb
//    1920x1080, 8 bins, 64 lights = 18.6 Mb
//    3840x2160, 2 bins, 64 lights = 18.7 Mb
//    3840x2160, 4 bins, 64 lights = 37.5 Mb
//    3840x2160, 8 bins, 64 lights = 74.9 Mb

NVLC_API NvlcContext			NVLC_Init(const sNvlcInit* pInput);

// IMPORTANT: NVLC (DX12) doesn't track usage of the resources, because NVLC doesn't use any synchronization internally except
// resource barriers. The app should ensure that resources are not in use before calling NVLC_Free().

NVLC_API void					NVLC_Free(NvlcContext context);

// CPU side - should be called at "frame prepare" step. This function doesn't instroduce any GPU work (except some resource updates),
// but does lights frustum culling and pre-classification on the CPU. It fills out "output" fields in "sNvlcPerGroupInputOutput".
// Performance (CPU i7-4770K):
//     1000 omnis - 0.013 ms
//     1000 spots - 0.026 ms
// IMPORTANT: A user must reorganize light data for lighting phase using remap buffer!

NVLC_API bool					NVLC_Prepare(NvlcContext context, const sNvlcPrepare* pInput);

// GPU side - should be called after depth buffer fill pass. It computes NVLC_TILE_DATA and NVLC_LIGHT_LIST textures,
// which can be queried using NVLC_GetTexture function and used in lighting phase.

NVLC_API bool					NVLC_Compute(NvlcContext context, const sNvlcCompute* pInput);

NVLC_API sNvlcShaderResource	NVLC_GetTexture(NvlcContext context, eNvlcTextureName name);
NVLC_API bool					NVLC_GetDims(NvlcContext context_or_null, sNvlcDimensions* pOut);
NVLC_API const char*			NVLC_GetVersionString();

#pragma pack(pop)
