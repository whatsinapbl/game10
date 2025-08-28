typedef float2 vec2;
typedef float3 vec3;
typedef float4 vec4;
typedef float2x2 mat2;
typedef float3x3 mat3;
typedef float4x4 mat4;

#define cb ConstantBuffer
#define sb StructuredBuffer
#define pi 3.14159265359

vec4 operator*(mat4 m, vec4 v) { return mul(m, v); }

void vsTriangle(uint id: SV_VertexID,
                out vec4 svp: SV_Position,
                out vec3 color: color)
{
   vec2 points[] = {
      { 0, 0.5 },
      { 0.5, -0.5 },
      { -0.5, -0.5 },
   };
   vec3 colors[] = {
      { 1, 0, 0 },
      { 0, 1, 0 },
      { 0, 0, 1 }
   };
   svp = vec4(points[id], 0.5, 1);
   color = colors[id];
}

void psTriangle(vec4 svp: SV_Position,
                vec3 color: color,
                out vec3 target: SV_Target)
{
   target = color;
}

void vsMain(uint id: SV_VertexID,
            sb<vec3> points,
            cb<mat4> model,
            cb<mat4> view,
            cb<mat4> proj,
            out vec4 svp: SV_Position)
{
   svp = proj * view * model * vec4(points[id], 1);
}

void psMain(vec4 svp: SV_Position,
            out vec3 target: SV_Target)
{
   target = vec3(1, 1, 0.5);
}
