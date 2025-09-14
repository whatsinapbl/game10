#pragma pack_matrix(row_major)
#ifdef SLANG_HLSL_ENABLE_NVAPI
#include "nvHLSLExtns.h"
#endif

#ifndef __DXC_VERSION_MAJOR
// warning X3557: loop doesn't seem to do anything, forcing loop to unroll
#pragma warning(disable : 3557)
#endif


#line 90 "core"
Texture2D<float4 > entryPointParams_diffuse_0 : register(t0);


#line 890 "hlsl.meta.slang"
SamplerState entryPointParams_sampler_0 : register(s0);


#line 4313 "core.meta.slang"
cbuffer entryPointParams_light_0 : register(b0)
{
    float3 entryPointParams_light_0;
}

#line 113 "shaders.hlsl"
void psMain(float4 svp_0 : SV_Position, float3 normal_0 : normal, float2 uv_0 : uv, out float4 target_0 : SV_Target)
{

#line 121
    target_0 = (0.75f * ((dot(normalize(normal_0), entryPointParams_light_0) + 1.0f) / 2.0f) + 0.25f) * entryPointParams_diffuse_0.Sample(entryPointParams_sampler_0, uv_0);
    return;
}

