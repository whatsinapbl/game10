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
mat4 operator*(mat4 a, mat4 b) { return mul(a, b); }
vec3 operator*(mat3 m, vec3 v) { return mul(m, v); }
mat3 operator*(mat3 a, mat3 b) { return mul(a, b); }

void vsTriangle(uint id: SV_VertexID,
                cb<mat4> model,
                cb<mat4> view,
                cb<mat4> proj,
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
   svp = proj * view * model * vec4(points[id], 1, 1);
   color = colors[id];
}

void psTriangle(vec4 svp: SV_Position,
                vec3 color: color,
                out vec3 target: SV_Target)
{
   target = color;
}

void vsShadow(uint id: SV_VertexID,
              sb<vec3> points,
              cb<mat4> model,
              cb<mat4> cascade,
              out vec4 svp: SV_Position)
{
   svp = cascade * model * vec4(points[id], 1);
}

mat3 inverse(mat3 m)
{
   float det = dot(m[0], cross(m[1], m[2]));

   mat3 res = {
      m[1][1] * m[2][2] - m[2][1] * m[1][2],
      m[0][2] * m[2][1] - m[0][1] * m[2][2],
      m[0][1] * m[1][2] - m[0][2] * m[1][1],
      m[1][2] * m[2][0] - m[1][0] * m[2][2],
      m[0][0] * m[2][2] - m[0][2] * m[2][0],
      m[1][0] * m[0][2] - m[0][0] * m[1][2],
      m[1][0] * m[2][1] - m[2][0] * m[1][1],
      m[2][0] * m[0][1] - m[0][0] * m[2][1],
      m[0][0] * m[1][1] - m[1][0] * m[0][1]
   };
   return res / det;
}

void vsMain(uint id: SV_VertexID,
            sb<vec3> points,
            sb<vec3> normals,
            sb<vec2> uvs,
            cb<mat4> model,
            cb<mat4> view,
            cb<mat4> proj,
            out vec4 svp: SV_Position,
            out vec3 normal: normal,
            out vec2 uv: uv)
{
   svp = proj * view * model * vec4(points[id], 1);
   normal = inverse(transpose(mat3(model))) * normals[id];
   uv = uvs[id];
}

float linearize(float x)
{
   if (x <= 0.04045f)
      return x / 12.92f;
   else
      return pow((x + 0.055f) / 1.055f, 2.4f);
}

vec3 linearize(vec3 x)
{
   vec3 res = { linearize(x[0]), linearize(x[1]), linearize(x[2]) };
   return res;
}

vec3 rgba(float r, float g, float b, float a)
{
   vec3 res = { r / 255, g / 255, b / 255 };
   return linearize(res);
}

void psMain(vec4 svp: SV_Position,
            vec3 normal: normal,
            vec2 uv: uv,
            Texture2D<vec3> diffuse,
            SamplerState sampler,
            cb<vec3> light,
            out vec3 target: SV_Target)
{
   normal = normalize(normal);
   float threshold = 0.25;
   // vec3 light = normalize(vec3(1, 1, 1));
   float lambert = (1 - threshold) * ((dot(normal, light) + 1) / 2) + threshold;
   target = lambert * diffuse.Sample(sampler, uv);
}
