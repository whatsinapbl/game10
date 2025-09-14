#pragma pack_matrix(row_major)
#ifdef SLANG_HLSL_ENABLE_NVAPI
#include "nvHLSLExtns.h"
#endif

#ifndef __DXC_VERSION_MAJOR
// warning X3557: loop doesn't seem to do anything, forcing loop to unroll
#pragma warning(disable : 3557)
#endif


#line 49 "shaders.hlsl"
void psTriangle(float4 svp_0 : SV_Position, float3 color_0 : color, out float3 target_0 : SV_Target)
{


    target_0 = color_0;
    return;
}

