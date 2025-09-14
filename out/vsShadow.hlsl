#pragma pack_matrix(row_major)
#ifdef SLANG_HLSL_ENABLE_NVAPI
#include "nvHLSLExtns.h"
#endif

#ifndef __DXC_VERSION_MAJOR
// warning X3557: loop doesn't seem to do anything, forcing loop to unroll
#pragma warning(disable : 3557)
#endif


#line 56 "shaders.hlsl"
StructuredBuffer<float3 > entryPointParams_points_0 : register(t0);


#line 56
cbuffer entryPointParams_model_0 : register(b0)
{
    float4x4 entryPointParams_model_0;
}

#line 56
cbuffer entryPointParams_cascade_0 : register(b1)
{
    float4x4 entryPointParams_cascade_0;
}

#line 18
float4x4 x2A_0(float4x4 a_0, float4x4 b_0)
{
    return mul(a_0, b_0);
}


#line 14
float4 x2A_1(float4x4 m_0, float4 v_0)
{
    return mul(m_0, v_0);
}


#line 56
void vsShadow(uint id_0 : SV_VertexID, out float4 svp_0 : SV_Position)
{



    svp_0 = x2A_1(x2A_0(entryPointParams_cascade_0, entryPointParams_model_0), float4(entryPointParams_points_0.Load(id_0), 1.0f));
    return;
}

