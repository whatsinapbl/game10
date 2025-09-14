#pragma pack_matrix(row_major)
#ifdef SLANG_HLSL_ENABLE_NVAPI
#include "nvHLSLExtns.h"
#endif

#ifndef __DXC_VERSION_MAJOR
// warning X3557: loop doesn't seem to do anything, forcing loop to unroll
#pragma warning(disable : 3557)
#endif


#line 4313 "core.meta.slang"
cbuffer entryPointParams_model_0 : register(b0)
{
    float4x4 entryPointParams_model_0;
}

#line 4313
cbuffer entryPointParams_view_0 : register(b1)
{
    float4x4 entryPointParams_view_0;
}

#line 4313
cbuffer entryPointParams_proj_0 : register(b2)
{
    float4x4 entryPointParams_proj_0;
}

#line 18 "shaders.hlsl"
float4x4 x2A_0(float4x4 a_0, float4x4 b_0)
{
    return mul(a_0, b_0);
}


#line 14
float4 x2A_1(float4x4 m_0, float4 v_0)
{
    return mul(m_0, v_0);
}


#line 31
void vsTriangle(uint id_0 : SV_VertexID, out float4 svp_0 : SV_Position, out float3 color_0 : color)
{

#line 31
    uint _S1 = id_0;



    float2  points_0[int(3)] = { float2(0.0f, 0.5f), float2(0.5f, -0.5f), float2(-0.5f, -0.5f) };

#line 40
    float3  colors_0[int(3)] = { float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), float3(0.0f, 0.0f, 1.0f) };

#line 45
    svp_0 = x2A_1(x2A_0(x2A_0(entryPointParams_proj_0, entryPointParams_view_0), entryPointParams_model_0), float4(points_0[id_0], 1.0f, 1.0f));
    color_0 = colors_0[_S1];
    return;
}

