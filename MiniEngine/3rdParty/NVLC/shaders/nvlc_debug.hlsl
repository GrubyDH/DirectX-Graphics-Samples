#ifndef NV_DEBUG_HLSL
#define NV_DEBUG_HLSL

#ifndef NVLC_TILE_DIM_X
	#error "NVLC_TILE_DIM_X must be defined!"
#endif

#ifndef NVLC_TILE_DIM_Y
	#error "NVLC_TILE_DIM_X must be defined!"
#endif

//=========================================================================================
// USAGE
//=========================================================================================
/*

Function:

	uint NvPrint_{f/i/ui}(float/int/uint x, uint2 origin, uint2 uv, uint scale);

Input:

	x      - value to print
	origin - text box origin in pixels (top-right point for D3D. bottom-right for OGL)
	uv     - integer pixel coordinates (SV_Position.xy or SV_DispatchThreadID)
	scale  - text size in pixels (scale = 1 - no scale)

Output:

	NV_FOREGROUND bit - shows that text is present in current pixel
	NV_BACKGROUND bit - shows that text box is present in current pixel (you can skip this
						   if solid text box is not required)

Limitations:

	- Printed value *must be* the same for each pixel (thread) inside a text box
	- "0" can't be printed

Good practice:

	- print something from any buffers or textures
	- print tile info in tiled shading
	- NvPrint can be called multiply times

Bad practice:

	- print shader local values
	- print pixel derivatives

//=========================================================================================

float3 NvShowTileInfo(uint colorize_me, uint print_me, uint2 uv)

Input:

	colorize_me - tile light count (can be different per pixel / thread in a tile, if
	              you use "half-z split list" solution, for example)
	print_me    - tile light count (can be equal to colorize_me if not divergent,
	              otherwise, use maximum value or modify the function to show two values)
	uv          - integer pixel coordinates (SV_Position.xy or SV_DispatchThreadID, same as
	              for NvPrint)

Limitations:

	Designed for tile size >= 16 pixels

*/

//=========================================================================================
// CUSTOM FONT
//=========================================================================================

#define X										1
#define _										0

#define ROW(a, b, c, d)						((d << 3) | (c << 2) | (b << 1) | a)

#define CHAR_W								4
#define CHAR_H								6

#define CHAR_MINUS							10
#define CHAR_DOT								11
#define MAX_CHARS								12

STATIC_CONST uint g_NvFont[CHAR_H * MAX_CHARS] =
{
	ROW(_, X, X, _),
	ROW(X, _, _, X),
	ROW(X, _, _, X),
	ROW(X, _, _, X),
	ROW(X, _, _, X),
	ROW(_, X, X, _),

	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),

	ROW(X, X, X, _),
	ROW(_, _, _, X),
	ROW(X, X, X, X),
	ROW(X, _, _, _),
	ROW(X, _, _, _),
	ROW(X, X, X, X),

	ROW(X, X, X, _),
	ROW(_, _, _, X),
	ROW(_, X, X, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(X, X, X, _),

	ROW(X, _, _, X),
	ROW(X, _, _, X),
	ROW(X, X, X, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),

	ROW(X, X, X, _),
	ROW(X, _, _, _),
	ROW(X, X, X, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(X, X, X, X),

	ROW(_, X, X, X),
	ROW(X, _, _, _),
	ROW(X, X, X, X),
	ROW(X, _, _, X),
	ROW(X, _, _, X),
	ROW(X, X, X, _),

	ROW(_, X, X, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),

	ROW(X, X, X, X),
	ROW(X, _, _, X),
	ROW(X, X, X, X),
	ROW(X, _, _, X),
	ROW(X, _, _, X),
	ROW(X, X, X, X),

	ROW(X, X, X, X),
	ROW(X, _, _, X),
	ROW(X, X, X, X),
	ROW(_, _, _, X),
	ROW(_, _, _, X),
	ROW(X, X, X, _),

	ROW(_, _, _, _),
	ROW(_, _, _, _),
	ROW(_, _, _, _),
	ROW(X, X, X, X),
	ROW(_, _, _, _),
	ROW(_, _, _, _),

	ROW(_, _, _, _),
	ROW(_, _, _, _),
	ROW(_, _, _, _),
	ROW(_, _, _, _),
	ROW(_, _, _, _),
	ROW(_, _, _, X),
};

#undef X
#undef _
#undef ROW

//=========================================================================================
// PRIVATE
//=========================================================================================

#define NV_NONE					0x0
#define NV_BACKGROUND			0x1
#define NV_FOREGROUND			0x2

#define NV_PRINT_ITERATION(ch)	\
	{ \
		r.x += r.z; \
		if( r.x < r.z ) \
		{ \
			r.xy /= scale; \
			mask = (g_NvFont[r.y + ch * CHAR_H] & (1 << r.x)) != 0 ? NV_FOREGROUND : NV_BACKGROUND; \
		} \
	}

uint4 _NvPrint_Init(uint2 uv, uint2 origin, uint scale)
{
	uint4 r;

	r.xy = uv - origin;
	r.zw = uint2(CHAR_W + 1, CHAR_H) * scale;

	return r;
}

uint _NvPrint_ui(inout uint4 r, uint x, uint scale)
{
	uint mask = NV_NONE;

	while( x > 0 && mask == NV_NONE )
	{
		uint n = x % 10;
		x /= 10;

		NV_PRINT_ITERATION(n);
	}

	return mask;
}

uint NvPrint_ui(uint x, uint2 origin, uint2 uv, uint scale)
{
	uint4 r = _NvPrint_Init(uv, origin, scale);
	uint mask = NV_NONE;

	if( r.y < r.w )
		mask = _NvPrint_ui(r, x, scale);

	return mask;
}

uint NvPrint_i(int x, uint2 origin, uint2 uv, uint scale)
{
	uint4 r = _NvPrint_Init(uv, origin, scale);
	uint mask = NV_NONE;

	if( r.y < r.w )
	{
		mask = _NvPrint_ui(r, abs(x), scale);

		if( x < 0 )
			NV_PRINT_ITERATION( CHAR_MINUS );
	}

	return mask;
}

uint NvPrint_f(float x, uint2 origin, uint2 uv, uint scale)
{
	uint4 r = _NvPrint_Init(uv, origin, scale);
	uint mask = NV_NONE;

	uint a = uint( floor( abs(x) ) );
	uint b = uint( frac( abs(x) ) * 10000.0 );

	if( r.y < r.w )
	{
		mask = _NvPrint_ui(r, b, scale);
		NV_PRINT_ITERATION( CHAR_DOT );
		mask |= _NvPrint_ui(r, a, scale);

		if( x < 0 )
			NV_PRINT_ITERATION( CHAR_MINUS );
	}

	return mask;
}

//=========================================================================================

#define NV_COLORS_NUM			15

STATIC_CONST float3 g_NvColor[NV_COLORS_NUM] =
{
	float3(0.000, 0.000, 1.000),
	float3(0.000, 0.133, 1.000),
	float3(0.000, 0.410, 1.000),
	float3(0.000, 0.859, 1.000),
	float3(0.000, 0.825, 0.500),
	float3(0.000, 0.800, 0.125),
	float3(0.000, 0.700, 0.000),
	float3(0.100, 0.700, 0.000),
	float3(0.200, 0.800, 0.000),
	float3(0.351, 0.859, 0.000),
	float3(0.612, 0.859, 0.000),
	float3(1.000, 0.859, 0.000),
	float3(1.000, 0.410, 0.000),
	float3(1.000, 0.133, 0.000),
	float3(1.000, 0.000, 0.000)
};

float3 NvShowTileInfo(uint2 uv, uint colorize_me, uint print_me, uint maximum, bool bOverflow)
{
	int2 local_uv = int2( uv % uint2(NVLC_TILE_DIM_X, NVLC_TILE_DIM_Y) );

	// NOTE: color

	float3 color;

	if( bOverflow )
		color = (local_uv.x == local_uv.y || local_uv.x == NVLC_TILE_DIM_X - local_uv.y) ? float3(0.5, 0.5, 0.5) : float3(1.0, 1.0, 1.0);
	else if( colorize_me == 0 )
		color = float3(0.000, 0.000, 0.000);
	else
	{
		float f = log2( float(colorize_me) ) / log2( max(float(maximum), 1.001) );
		uint i = uint( float(NV_COLORS_NUM - 0.001) * f );

		color = g_NvColor[i];
	}

	// NOTE: border

	float border = (local_uv.x == NVLC_TILE_DIM_X - 1 || local_uv.y == NVLC_TILE_DIM_Y - 1) ? 1.0 : 0.0;
	color *= lerp(1.0, 0.75, border);

	// NOTE: numbers

	uint mask = NvPrint_ui(print_me, uint2(NVLC_TILE_DIM_X - 1, 1), local_uv, 1);

	color *= (mask & NV_FOREGROUND) != 0 ? 0.15 : 1.0;

	return color;
}

float3 NvSoftBlend(float3 a, float3 b)
{
	float3 t = 1.0 - 2.0 * b;
	float3 c = (2.0 * b + a * t) * a;
	float3 d = 2.0 * a * (1.0 - b) - sqrt(a) * t;

	return lerp( c, d, step(0.5, b) );
}

#endif
