#ifndef NVLC_DEFINES_H
#define NVLC_DEFINES_H

#define NVLC_D3D										0
#define NVLC_GL											1

// NOTE: dimensions

#define NVLC_TILE_DIM_X									16
#define NVLC_TILE_DIM_Y									16
#define NVLC_MAX_POSSIBLE_LIGHTS_PER_BIN				256
#define NVLC_MAX_BINS									8

// Derived
	
#define NVLC_BINS_MASK									(NVLC_MAX_BINS - 1)
#define NVLC_ALL_BIN_BITS								((1 << NVLC_MAX_BINS) - 1)

#define NVLC_END_OF_LIST								0xFFFF
#define NVLC_END_OF_GROUP								0xFFFE

#endif
