#ifndef NVLC_HLSL
#define NVLC_HLSL

//====================================================================================================================================
// INPUTS
//====================================================================================================================================

// viewz
//   linear view z, can be:
//   CS - linearized from HW depth
//   PS - SV_Position.w

// uiGridWidth
//   tile grid width, can be get using NVLC_GetDims()

// IMPORTANT:

// - NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN "must" be redefined to the actual maximum count of supported
//   lights per bin to save some shared memory (only if NVLC_SHARED_MEMORY_UINTS == 1) and get proper
//   addressing

// - NVLC_SHARED_MEMORY_UINTS should be set to "maximum number of uints available per bin" if NVLC is
//   used in a compute shader. It unlocks a lot of performance! NVLC_SHARED_MEMORY_UINTS = 0 means that
//   NVLC is used inside a pixel shader

// - If NVLC_DEBUG_BINS is "0" light counts per tile are shown by NVLC_TILE_INFO, otherwise - how
//   many bins are used per tile is shown

// - NVLC_NVIDIA_EXTENSIONS can be set to 1 if NVIDIA intrinsics are available

//====================================================================================================================================
// PRIVATE
//====================================================================================================================================

//#define NVLC_SKIP_NEXT_INDEX_PRELOAD

#ifndef NVLC_DEFINES_H
	#error "'nvlc_defines.h' has not been included *before* 'nvlc.h'!"
#endif

#if( !defined NVLC_BINS || (NVLC_BINS != 1 && NVLC_BINS != 2 && NVLC_BINS != 4 && NVLC_BINS != 8) )
	#error "NVLC_BINS must be defined as 1/2/4/8!"
#endif

#if( !defined NVLC_DEBUG_BINS || NVLC_DEBUG_BINS < 0 || NVLC_DEBUG_BINS > 1  )
	#error "NVLC_DEBUG_BINS must be defined as 0/1!"
#endif

#if( !defined NVLC_SHARED_MEMORY_UINTS || NVLC_SHARED_MEMORY_UINTS < 0 )
	#error "NVLC_SHARED_MEMORY_UINTS must be defined as >= 0!"
#endif

#if( !defined NVLC_NVIDIA_EXTENSIONS || NVLC_NVIDIA_EXTENSIONS < 0 || NVLC_NVIDIA_EXTENSIONS > 1 )
	#error "NVLC_NVIDIA_EXTENSIONS must be defined as 0/1!"
#else
	#if( NVLC_NVIDIA_EXTENSIONS == 1 && NVLC_BINS != 1 )
		#error "NVLC_NVIDIA_EXTENSIONS = 1 requires NVLC_BINS = 1!"
	#endif
#endif

#if( !defined NVLC_TEX2D_TILE_DATA )
	#error "NVLC_TEX2D_TILE_DATA must be defined as 2D texture (float4)!"
#endif

#if( !defined NVLC_BUF_LIGHT_LIST )
	#error "NVLC_BUF_LIGHT_LIST must be defined as Buffer texture (uint)!"
#endif

#include "nvlc_crossplatform.hlsl"
#include "nvlc_debug.hlsl"

struct sNvlcTile
{
	float	zNear;
	float	zFar;
	float	zFar_scattering;
	uint	lightsInWorstBin;
	uint	lengthOfWorstBin;
	uint	logMaxBins;
	bool	overflow;
};

sNvlcTile _NVLC_GetTileData(uint2 tileId)
{
	uint4 t = texelFetch_2d(NVLC_TEX2D_TILE_DATA, int2(tileId), 0);

	sNvlcTile tile;
	tile.zNear				= abs( asfloat(t.x) );
	tile.zFar				= asfloat( t.y | NVLC_BINS_MASK );
	tile.zFar_scattering	= asfloat(t.z);
	tile.lightsInWorstBin	= t.w & 0xFFFF;
	tile.lengthOfWorstBin	= t.w >> 16;
	tile.logMaxBins			= t.y & NVLC_BINS_MASK;
	tile.overflow			= asfloat(t.x) < 0.0;

	return tile;
}

uint _NVLC_GetBin(float viewz, sNvlcTile tile)
{
	#if( NVLC_BINS > 1 )

		float f = saturate( (abs(viewz) - tile.zNear) / (tile.zFar - tile.zNear) );
		float bin = min(f, 0.999999) * float(1 << tile.logMaxBins);

		return uint(bin);

	#else

		return 0;

	#endif
}


#if( NVLC_SHARED_MEMORY_UINTS != 0 )
	groupshared uint nvlc_shared_list[NVLC_BINS * NVLC_SHARED_MEMORY_UINTS];
#endif

#if( NVLC_NVIDIA_EXTENSIONS == 1 )

	// IMPORTANT: these functions ignore "helper" lanes in graphics mode

	uint NvLaneMask()
	{
		#if( NVLC_SHARED_MEMORY_UINTS != 0 )
			return 0xFFFFFFFF;
		#else
			return NvBallot(1);
		#endif
	}

	uint NvLanes(uint laneMask)
	{
		#if( NVLC_SHARED_MEMORY_UINTS != 0 )
			return NV_WARP_SIZE;
		#else
			return countbits(laneMask);
		#endif
	}

	uint NvLaneIndex(uint laneMask)
	{
		#if( NVLC_SHARED_MEMORY_UINTS != 0 )
			return NvGetLaneId();
		#else
			uint thisLaneMask = 1 << NvGetLaneId();
			return countbits(laneMask & (thisLaneMask - 1));
		#endif
	}

	uint NvShuffle(inout uint shuffleMask, uint shuffleIndex, uint value)
	{
		#if( NVLC_SHARED_MEMORY_UINTS == 0 )
			// NOTE: find next active lane from "shuffleMask" and set corresponding bit to zero (to find the following next time)
			shuffleIndex = firstbitlow(shuffleMask);
			shuffleMask ^= 1 << shuffleIndex;
		#endif

		return NvShfl(value, shuffleIndex);
	}

#endif

float3 _NVLC_TileInfo(uint2 globalId, sNvlcTile tile, uint i)
{
	if( tile.lightsInWorstBin == 0 )
		return float3(0.0, 0.0, 0.0);

	#if( NVLC_DEBUG_BINS == 1 )

		uint bins = 1 << tile.logMaxBins;

		return NvShowTileInfo(globalId, bins, bins, NVLC_MAX_BINS, false);

	#else

		// IMPORTANT: NVLC doesn't store light counts per bin (it is expensive), so we have to iterate to get the count...
		// NOTE: i = next + laneIndex

		uint lightsInBin = 0;

		#if( NVLC_NVIDIA_EXTENSIONS == 1 )

			uint laneMask = NvLaneMask();
			uint lanes = NvLanes(laneMask);
			uint ind;

			do
			{
				uint precachedIndices = texelFetch_buf(NVLC_BUF_LIGHT_LIST, int(i)).x;
				uint shuffleMask = laneMask;
				uint shuffleIndex = 0;

				do
				{
					ind = NvShuffle(shuffleMask, shuffleIndex++, precachedIndices);
					lightsInBin += ind < NVLC_END_OF_GROUP ? 1 : 0;
				}
				while( shuffleIndex != lanes && ind != NVLC_END_OF_LIST );

				i += shuffleIndex;
			}
			while( ind != NVLC_END_OF_LIST );

		#elif( NVLC_SHARED_MEMORY_UINTS > 0 )

			uint next = i * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN;

			while( nvlc_shared_list[next] != NVLC_END_OF_LIST )
				lightsInBin += nvlc_shared_list[next++] < NVLC_END_OF_GROUP ? 1 : 0;

		#else

			uint ind;

			do
			{
				ind = texelFetch_buf(NVLC_BUF_LIGHT_LIST, int(i++)).x;
				lightsInBin += ind < NVLC_END_OF_GROUP ? 1 : 0;
			}
			while( ind != NVLC_END_OF_LIST );

		#endif

		return NvShowTileInfo(globalId, lightsInBin, tile.lightsInWorstBin, NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN, tile.overflow);

	#endif
}

//====================================================================================================================================
// MAIN INTERFACE
//====================================================================================================================================

//====================================================================================================================================
#if( NVLC_NVIDIA_EXTENSIONS == 1 )

	#define NVLC_PRE_SHADER(tileId, unused_threadId, unused_uiGridWidth) \
		sNvlcTile nvlc_tile = _NVLC_GetTileData(tileId)

	#define NVLC_INIT(viewz, tileId, uiGridWidth) \
		uint nvlc_laneMask = NvLaneMask(); \
		uint nvlc_lanes = NvLanes(nvlc_laneMask); \
		uint nvlc_next = _NVLC_GetBin(viewz, nvlc_tile); \
		nvlc_next = ((tileId.y * uiGridWidth + tileId.x) * NVLC_BINS + nvlc_next) * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN; \
		nvlc_next += NvLaneIndex(nvlc_laneMask); \
		uint NVLC_LIGHT_ID

	#define NVLC_LOOP_BEGIN \
		do \
		{ \
			uint nvlc_precachedIndices = texelFetch_buf(NVLC_BUF_LIGHT_LIST, int(nvlc_next)).x; \
			uint shuffleMask = nvlc_laneMask; \
			uint nvlc_shuffleIndex = 0; \
			NVLC_LIGHT_ID = NvShuffle(shuffleMask, 0, nvlc_precachedIndices); \
			while( NVLC_LIGHT_ID < NVLC_END_OF_GROUP && nvlc_shuffleIndex < nvlc_lanes ) \
			{ \

	#define NVLC_LOOP_END \
				NVLC_LIGHT_ID = NvShuffle(shuffleMask, ++nvlc_shuffleIndex, nvlc_precachedIndices); \
			} \
			nvlc_next += nvlc_shuffleIndex; \
		} \
		while( NVLC_LIGHT_ID < NVLC_END_OF_GROUP ); \
		nvlc_next += NVLC_LIGHT_ID != NVLC_END_OF_LIST ? 1 : 0\

//====================================================================================================================================
#elif( NVLC_SHARED_MEMORY_UINTS > 0 )

	sNvlcTile _NVLC_LoadLightListToSharedMemory(uint2 tileId, uint threadId, uint uiGridWidth)
	{
		sNvlcTile tile = _NVLC_GetTileData(tileId);

		if( threadId <= tile.lengthOfWorstBin )
		{
			UNROLL
			for( uint bin = 0; bin < NVLC_BINS; bin++ )
			{
				uint src = ((tileId.y * uiGridWidth + tileId.x) * NVLC_BINS + bin) * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN + threadId;
				uint dst = bin * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN + threadId;

				nvlc_shared_list[dst] = texelFetch_buf(NVLC_BUF_LIGHT_LIST, int(src)).x;
			}
		}

		GroupMemoryBarrierWithGroupSync();

		return tile;
	}

	#define NVLC_PRE_SHADER(tileId, threadId, uiGridWidth) \
		sNvlcTile nvlc_tile = _NVLC_LoadLightListToSharedMemory(tileId, threadId, uiGridWidth)

	#define NVLC_INIT(viewz, unused_tileId, unused_uiGridWidth) \
		uint nvlc_bin = _NVLC_GetBin(viewz, nvlc_tile); \
		uint nvlc_next = nvlc_bin * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN; \
		uint NVLC_LIGHT_ID = 0

	#define NVLC_LOOP_BEGIN \
		if( NVLC_LIGHT_ID != NVLC_END_OF_LIST ) \
			NVLC_LIGHT_ID = nvlc_shared_list[nvlc_next++]; \
		while( NVLC_LIGHT_ID < NVLC_END_OF_GROUP ) \
		{

	#define NVLC_LOOP_END \
			NVLC_LIGHT_ID = nvlc_shared_list[nvlc_next++]; \
		}

//====================================================================================================================================
#else

	#define NVLC_PRE_SHADER(tileId, unused_threadId, unused_uiGridWidth) \
		sNvlcTile nvlc_tile = _NVLC_GetTileData(tileId);

	#if( !defined NVLC_SKIP_NEXT_INDEX_PRELOAD )

		#define NVLC_INIT(viewz, tileId, uiGridWidth) \
			uint nvlc_bin = _NVLC_GetBin(viewz, nvlc_tile); \
			uint nvlc_next = ((tileId.y * uiGridWidth + tileId.x) * NVLC_BINS + nvlc_bin) * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN; \
			uint nvlc_temp = 0

		#define NVLC_LOOP_BEGIN \
			if( nvlc_temp != NVLC_END_OF_LIST ) \
				nvlc_temp = texelFetch_buf(NVLC_BUF_LIGHT_LIST, int(nvlc_next++)).x; \
			while( nvlc_temp < NVLC_END_OF_GROUP ) \
			{ \
				uint NVLC_LIGHT_ID = nvlc_temp; \
				nvlc_temp = texelFetch_buf(NVLC_BUF_LIGHT_LIST, int(nvlc_next++)).x

		#define NVLC_LOOP_END \
			}

	#else

		#define NVLC_INIT(viewz, tileId, uiGridWidth) \
			uint nvlc_bin = _NVLC_GetBin(viewz, nvlc_tile); \
			uint nvlc_next = ((tileId.y * uiGridWidth + tileId.x) * NVLC_BINS + nvlc_bin) * NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN; \
			uint NVLC_LIGHT_ID

		#define NVLC_LOOP_BEGIN \
			NVLC_LIGHT_ID = texelFetch_buf(NVLC_BUF_LIGHT_LIST, int(nvlc_next++)).x; \
			while( NVLC_LIGHT_ID < NVLC_END_OF_GROUP ) \
			{

		#define NVLC_LOOP_END \
				NVLC_LIGHT_ID = texelFetch_buf(NVLC_BUF_LIGHT_LIST, int(nvlc_next++)).x; \
			}

	#endif

#endif

//====================================================================================================================================
// DEBUG UTILITIES
//====================================================================================================================================

#if( NVLC_NVIDIA_EXTENSIONS == 1 || NVLC_SHARED_MEMORY_UINTS == 0 )

	#define NVLC_TILE_INFO(globalId) \
		_NVLC_TileInfo(globalId, nvlc_tile, nvlc_next)

#else

	#define NVLC_TILE_INFO(globalId) \
		_NVLC_TileInfo(globalId, nvlc_tile, nvlc_bin)

#endif

#define NVLC_IS_EARLY_OUT \
	(nvlc_tile.lightsInWorstBin == 0)

#define NVLC_IS_OVERFLOW \
	nvlc_tile.overflow

#define NVLC_SOFT_BLEND \
	NvSoftBlend

#endif
