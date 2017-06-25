#ifndef NV_CROSSPLATFORM_HLSL
#define NV_CROSSPLATFORM_HLSL

// Cross language support

#ifndef SHADER_API
	#error "SHADER_API should be (0 - D3D, 1 - OGL)"
#endif

#define API_D3D														0
#define API_OGL														1

#if( SHADER_API == API_OGL )

	#define float2													vec2
	#define float3													vec3
	#define float4													vec4

	#define int2													ivec2
	#define int3													ivec3
	#define int4													ivec4

	#define uint2													uvec2
	#define uint3													uvec3
	#define uint4													uvec4

	#define bool2													bvec2
	#define bool3													bvec3
	#define bool4													bvec4

	#define float2x2												mat2x2
	#define float3x2												mat3x2
	#define float4x2												mat4x2
	#define float2x3												mat2x3
	#define float3x3												mat3x3
	#define float4x3												mat4x3
	#define float2x4												mat2x4
	#define float3x4												mat3x4
	#define float4x4												mat4x4

	#ifdef VERTEX_SHADER
		#define ATTR(slot, name)									layout(location = slot) in name;
		#define LINK(slot, name)									out name;
	#else
		#define LINK(slot, name)									in name;
	#endif

	#ifdef PIXEL_SHADER
		#define DRAW(slot, name)									layout(location = slot) out name;
	#endif

	#define VS_INPUT_BUILTIN
	#define VS_OUTPUT_BUILTIN										out gl_PerVertex { float4 gl_Position; float gl_ClipDistance[]; float gl_CullDistance[]; };
	#define VS_OUTPUT_CLIP_CULL(x)
	#define VS_MAIN(name)											SHADER_INPUT \
																	SHADER_OUTPUT \
																	void name()

	#define CLIP_DISTANCE(x)										gl_ClipDistance[x]
	#define CULL_DISTANCE(x)										gl_CullDistance[x]

	#define PS_INPUT_BUILTIN
	#define PS_OUTPUT_BUILTIN
	#define PS_MAIN(name)											SHADER_INPUT \
																	SHADER_OUTPUT \
																	void name()

	#define PS_MAIN_NULL(name)										SHADER_INPUT \
																	void name()

	#define CS_MAIN(name, x, y, z)									layout( local_size_x = x, local_size_y = y, local_size_z = z ) in; \
																	void name()

	#define DECLARE_CBUFFER(slot, name)								layout(shared, binding = slot) uniform name
	#define DECLARE_SBUFFER(slot, type, name)						layout(std430, binding = slot) buffer name ## _buf { type name[]; }
	#define DECLARE_RESOURCE(slot, type, name)						layout(binding = slot) uniform type name
	#define DECLARE_UAV(slot, type, format, name)					layout(binding = slot) layout(format) uniform type name

	#define UNROLL
	#define BRANCH
	#define LOOP
	#define FLATTEN
	#define EARLY_DEPTH_STENCIL										layout(early_fragment_tests) in;
	#define STATIC_CONST											uniform

	#define SWIZZLE_1(v)											v.xxxx
	#define SWIZZLE_2(v)											v.xyyy
	#define SWIZZLE_3(v)											v.xyzz

	#define texelFetch_ms(tex, uv, s)								texelFetch(tex, int2(uv), s)
	#define texelFetchOffset_ms(tex, uv, s, o)						texelFetch(tex, int2(uv + o), s)		// FIXME: texelFetchOffset with MSAA is missed!

	#define texelFetchOffset_2d(tex, uv, lod, o)					texelFetchOffset(tex, int2(uv), lod, o)

	#define texelFetch_buf(tex, uv)									texelFetch(tex, int(uv))
	#define texelFetch_1d(tex, uv, lod)								texelFetch(tex, int(uv), lod)
	#define texelFetch_2d(tex, uv, lod)								texelFetch(tex, int2(uv), lod)
	#define texelFetch_3d(tex, uv, lod)								texelFetch(tex, int3(uv), lod)

	#define textureSize_3d(tex, dims)								dims = textureSize(tex, 0)

	#define mul(m, v)													(m * v)
	#define mat3_mul_vec3(m, v)										mul(float3x3(m), v)
	#define saturate(x)												clamp(x, 0.0, 1.0)
	#define rsqrt													inversesqrt
	#define lerp													mix
	#define frac													fract
	#define asfloat													uintBitsToFloat
	#define asuint													floatBitsToUint
	#define firstbithigh											findMSB
	#define firstbitlow												findLSB
	#define countbits												bitCount
	#define InterlockedMin											atomicMin
	#define InterlockedMax											atomicMax
	#define InterlockedOr											atomicOr
	#define InterlockedExchange(mem, v, prev)						prev = atomicExchange(mem, v)
	#define InterlockedAdd(mem, v, prev)							prev = atomicAdd(mem, v)
	#define InterlockedAdd_tex(tex, uv, v, prev)					prev = imageAtomicAdd(tex, uv, v)
	#define GroupMemoryBarrierWithGroupSync							barrier
	#define GroupMemoryBarrier										memoryBarrierShared

	#define groupshared												shared
	#define nointerpolation											flat

	#ifdef PIXEL_SHADER

		#define ddx													dFdx
		#define ddy													dFdy

	#else

		#define ddx(x)												(x)
		#define ddx(x)												(x)
		#define fwidth(x)											(x)
		#define discard

	#endif

	#define f16tof32(p)												unpackHalf2x16(p).x
	#define f32tof16(p)												packHalf2x16( float2(p, 0.0) )

	#define NDC_UV(uv)												(uv * 0.5 + 0.5)

#else

	SamplerState 													sampler_Nearest : register(s0);

	#define sampler2DMS												Texture2DMS<float4>
	#define samplerBuffer											Buffer<float4>
	#define isamplerBuffer											Buffer<int4>
	#define usamplerBuffer											Buffer<uint4>
	#define sampler2D												Texture2D<float4>
	#define isampler2D												Texture2D<int4>
	#define usampler2D												Texture2D<uint4>
	#define sampler2DArray											Texture2DArray<float4>
	#define isampler2DArray											Texture2DArray<int4>
	#define usampler2DArray											Texture2DArray<uint4>
	#define imageBuffer												RWBuffer
	#define uimageBuffer											RWBuffer
	#define iimageBuffer											RWBuffer
	#define image2D													RWTexture2D
	#define uimage2D												RWTexture2D
	#define iimage2D												RWTexture2D
	#define image2DArray											RWTexture2DArray
	#define uimage2DArray											RWTexture2DArray
	#define iimage2DArray											RWTexture2DArray

	#define rgba32f													float4
	#define rgba32ui												uint4
	#define rgba32i													int4
	#define rgba8													unorm float4
	#define rg16ui													uint2
	#define r32f													float
	#define r32ui													uint
	#define r16ui													uint

	#ifdef VERTEX_SHADER
		#define ATTR(slot, name)									, name : POSITION ## slot
		#define LINK(slot, name)									, out name : TEXCOORD ## slot
	#else
		#define LINK(slot, name)									, in name : TEXCOORD ## slot
	#endif

	#ifdef PIXEL_SHADER
		#define DRAW(slot, name)									, name : SV_Target ## slot
	#endif

	#define VS_INPUT_BUILTIN										uint gl_InstanceID : SV_InstanceID, uint gl_VertexID : SV_VertexID
	#define VS_OUTPUT_BUILTIN										out float4 gl_Position : SV_Position
	#define VS_OUTPUT_CLIP_CULL(x)									, out float clipDistance_ ## x : SV_ClipDistance ## x, out float cullDistance_ ## x : SV_CullDistance ## x
	#define VS_MAIN(name)											void name(SHADER_INPUT, SHADER_OUTPUT)

	#define CLIP_DISTANCE(x)										clipDistance_ ## x
	#define CULL_DISTANCE(x)										cullDistance_ ## x

	#define PS_INPUT_BUILTIN										float4 gl_FragCoord : SV_Position
	#define PS_OUTPUT_BUILTIN										out float gl_FragDepth : SV_Depth
	#define PS_MAIN(name)											void name(SHADER_INPUT, SHADER_OUTPUT)
	#define PS_MAIN_NULL(name)										void name(SHADER_INPUT)

	#define CS_MAIN(name, x, y, z)									[numthreads(x, y, z)] \
																	void name(uint3 gl_GlobalInvocationID : SV_DispatchThreadID, uint3 gl_LocalInvocationID : SV_GroupThreadID, uint3 gl_WorkGroupID : SV_GroupID, uint gl_LocalInvocationIndex : SV_GroupIndex)

	#define DECLARE_CBUFFER(slot, name)								cbuffer name : register(b ## slot)
	#define DECLARE_SBUFFER(slot, type, name)						StructuredBuffer<type> name : register(t ## slot)
	#define DECLARE_RESOURCE(slot, type, name)						type name : register(t ## slot)
	#define DECLARE_UAV(slot, type, format, name)					type<format> name : register(u ## slot)

	#define UNROLL													[unroll]
	#define BRANCH													[branch]
	#define LOOP													[loop]
	#define FLATTEN													[flatten]
	#define EARLY_DEPTH_STENCIL										[earlydepthstencil]
	#define STATIC_CONST												static const

	#define SWIZZLE_1(v)											v
	#define SWIZZLE_2(v)											v
	#define SWIZZLE_3(v)											v

	#define mat3_mul_vec3(m, v)										mul((float3x3)m, v)
	#define imageStore(tex, uv, x)									tex[uv] = x
	#define imageLoad(tex, uv)										tex[uv]
	#define InterlockedAdd_tex(tex, uv, v, i)						InterlockedAdd(tex[uv], v, i)
	#define textureGather(tex, uv)									tex.GatherRed(sampler_Nearest, uv)
	#define textureGatherOffset(tex, uv, o)							tex.GatherRed(sampler_Nearest, uv, o)

	#define texelFetch_ms(tex, uv, s)								tex.Load(uv, s)
	#define texelFetchOffset_ms(tex, uv, s, o)						tex.Load(uv, s, o)

	#define texelFetchOffset_2d(tex, uv, lod, o)					tex.Load( uint3(uv, lod), o )

	#define texelFetch_buf(tex, uv)									tex.Load( uv )
	#define texelFetch_1d(tex, uv, lod)								tex.Load( uint2(uv, lod) )
	#define texelFetch_2d(tex, uv, lod)								tex.Load( uint3(uv, lod) )
	#define texelFetch_3d(tex, uv, lod)								tex.Load( uint4(uv, lod) )

	#define textureSize_3d(tex, dims)								tex.GetDimensions(dims.x, dims.y, dims.z)

	#define greaterThan(a, b)										(a > b)
	#define greaterThanEqual(a, b)									(a >= b)
	#define lessThan(a, b)											(a < b)
	#define lessThanEqual(a, b)										(a <= b)

	#define NDC_UV(uv)												(uv * float2(0.5, -0.5) + 0.5)

#endif

// FIXME: find a better place?

#define MATH_PI														3.14159265358979323846
#define WARP_SIZE													32
#define STATIC														// IMPORTANT: static branching will be removed by the compiler

#define sign_fast(x) 												(step(0.0, x) * 2.0 - 1.0)

#endif
