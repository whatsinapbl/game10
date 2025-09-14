#pragma pack_matrix(row_major)
#ifdef SLANG_HLSL_ENABLE_NVAPI
#include "nvHLSLExtns.h"
#endif

#ifndef __DXC_VERSION_MAJOR
// warning X3557: loop doesn't seem to do anything, forcing loop to unroll
#pragma warning(disable : 3557)
#endif


#line 93 "shaders.hlsl"
StructuredBuffer<float3 > entryPointParams_points_0 : register(t0);


#line 93
StructuredBuffer<float3 > entryPointParams_normals_0 : register(t1);


#line 93
cbuffer entryPointParams_model_0 : register(b0)
{
    float4x4 entryPointParams_model_0;
}

#line 93
cbuffer entryPointParams_view_0 : register(b1)
{
    float4x4 entryPointParams_view_0;
}

#line 93
cbuffer entryPointParams_proj_0 : register(b2)
{
    float4x4 entryPointParams_proj_0;
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


#line 64
float3x3 inverse_0(float3x3 m_1)
{

#line 79
    return float3x3(m_1[int(1)][int(1)] * m_1[int(2)][int(2)] - m_1[int(2)][int(1)] * m_1[int(1)][int(2)], m_1[int(0)][int(2)] * m_1[int(2)][int(1)] - m_1[int(0)][int(1)] * m_1[int(2)][int(2)], m_1[int(0)][int(1)] * m_1[int(1)][int(2)] - m_1[int(0)][int(2)] * m_1[int(1)][int(1)], m_1[int(1)][int(2)] * m_1[int(2)][int(0)] - m_1[int(1)][int(0)] * m_1[int(2)][int(2)], m_1[int(0)][int(0)] * m_1[int(2)][int(2)] - m_1[int(0)][int(2)] * m_1[int(2)][int(0)], m_1[int(1)][int(0)] * m_1[int(0)][int(2)] - m_1[int(0)][int(0)] * m_1[int(1)][int(2)], m_1[int(1)][int(0)] * m_1[int(2)][int(1)] - m_1[int(2)][int(0)] * m_1[int(1)][int(1)], m_1[int(2)][int(0)] * m_1[int(0)][int(1)] - m_1[int(0)][int(0)] * m_1[int(2)][int(1)], m_1[int(0)][int(0)] * m_1[int(1)][int(1)] - m_1[int(1)][int(0)] * m_1[int(0)][int(1)]) / dot(m_1[int(0)], cross(m_1[int(1)], m_1[int(2)]));
}


#line 22
float3 x2A_2(float3x3 m_2, float3 v_1)
{
    return mul(m_2, v_1);
}


#line 93
void vsTriplanar(uint id_0 : SV_VertexID, out float4 svp_0 : SV_Position, out float3 normal_0 : normal, out float2 uv_0 : uv, out float3 pos_0 : pos)
{

#line 93
    uint _S1 = id_0;

#line 99
    float4x4 _S2 = entryPointParams_model_0;

#line 99
    svp_0 = x2A_1(x2A_0(x2A_0(entryPointParams_proj_0, entryPointParams_view_0), entryPointParams_model_0), float4(entryPointParams_points_0.Load(id_0), 1.0f));
    normal_0 = normalize(x2A_2(inverse_0(transpose(float3x3(_S2[int(0)].xyz, _S2[int(1)].xyz, _S2[int(2)].xyz))), entryPointParams_normals_0.Load(_S1)));

    float3 p_0 = entryPointParams_points_0.Load(_S1);
    float3 n_0 = entryPointParams_normals_0.Load(_S1);
    float _S3 = abs(n_0.x);

#line 104
    float _S4 = abs(n_0.y);

#line 104
    bool _S5;

#line 104
    if(_S3 > _S4)
    {

#line 104
        _S5 = _S3 > (abs(n_0.z));

#line 104
    }
    else
    {

#line 104
        _S5 = false;

#line 104
    }

#line 104
    if(_S5)
    {

#line 105
        uv_0 = float2(p_0.y, p_0.z);

#line 104
    }
    else
    {

#line 106
        if(_S4 > _S3)
        {

#line 106
            _S5 = _S4 > (abs(n_0.z));

#line 106
        }
        else
        {

#line 106
            _S5 = false;

#line 106
        }

#line 106
        if(_S5)
        {

#line 107
            uv_0 = float2(p_0.x, p_0.z);

#line 106
        }
        else
        {
            uv_0 = float2(p_0.x, p_0.y);

#line 106
        }

#line 104
    }

#line 110
    pos_0 = x2A_1(_S2, float4(entryPointParams_points_0.Load(_S1), 1.0f)).xyz;
    return;
}

