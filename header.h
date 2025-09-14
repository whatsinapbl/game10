
#pragma region includes
#define NOMINMAX
#include <audioclient.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <hidusage.h>
#include <mmdeviceapi.h>
#include <tiny_gltf.h>
#include <wrl/client.h>

#include <SimpleMath.h>
#include <SpriteFont.h>
#include <WaveFrontReader.h>

#include <array>
#include <cstdint>
#include <map>
#include <numbers>
#include <print>
#include <vector>
#pragma endregion

#undef near

using namespace std;
using namespace DX;
using namespace DirectX;
using namespace SimpleMath;

#define null nullptr
#define CAT(left, right) left##right
#define SELECT(name, num) CAT(name##_, num)
#define GET_COUNT(_1, _2, _3, _4, _5, _6, count, ...) count
#define VA_SIZE(...) GET_COUNT(__VA_ARGS__, 6, 5, 4, 3, 2, 1)
#define VA_SELECT(name, ...) SELECT(name, VA_SIZE(__VA_ARGS__))(__VA_ARGS__)
#define range(...) VA_SELECT(range, __VA_ARGS__)
#define range_2(i, n) for (int i = 0; i < n; i++)
#define range_3(i, a, b) for (int i = a; i < b; i++)
#define irange(i, a, b) for (int i = a; i <= b; i++)
#define self (*this)
#define pi 3.1415927f

// name is either attributes["x"] or indices
#define PRIMITIVE(m, index) m.meshes[index].primitives[0]
#define ACCESSOR(m, index, name) m.accessors[PRIMITIVE(m, index).name]
#define BUFFER_VIEW(m, index, name) m.bufferViews[ACCESSOR(m, index, name).bufferView]
#define BUFFER(m, index, name) m.buffers[BUFFER_VIEW(m, index, name).buffer]

#define DATA(m, index, name) (BUFFER(m, index, name).data.data() + BUFFER_VIEW(m, index, name).byteOffset)
#define LEN(m, index, name) u32(BUFFER_VIEW(m, index, name).byteLength)
#define COUNT(m, index, name) ACCESSOR(m, index, name).count

#define BIT_WIDTH(x) u32(bit_width(u32(x)))

template <class T>
T* operator&(T&& x)
{
   return addressof(x);
}

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

template <class T>
struct ComPtr : public Microsoft::WRL::ComPtr<T>
{
   operator T*()
   {
      return self.Get();
   }
};

#pragma region math
template <int N>
struct vec
{
   float data[N];

   float& operator[](int i)
   {
      return data[i];
   }

   void operator+=(vec other)
   {
      self = self + other;
   }
   void operator-=(vec other)
   {
      self = self - other;
   }
};

template <int N>
vec<N> operator-(vec<N> v)
{
   vec<N> res;
   range (i, N) res[i] = -v[i];
   return res;
}

template <int N>
vec<N> operator+(vec<N> v, vec<N> w)
{
   vec<N> res;
   range (i, N) res[i] = v[i] + w[i];
   return res;
}

template <int N>
vec<N> operator-(vec<N> v, vec<N> w)
{
   vec<N> res;
   range (i, N) res[i] = v[i] - w[i];
   return res;
}

template <int N>
vec<N> operator*(vec<N> v, float factor)
{
   vec<N> res;
   range (i, N) res[i] = v[i] * factor;
   return res;
}

template <int N>
vec<N> operator*(float factor, vec<N> v)
{
   vec<N> res;
   range (i, N) res[i] = factor * v[i];
   return res;
}

template <int N>
vec<N> operator/(vec<N> v, float factor)
{
   vec<N> res;
   range (i, N) res[i] = v[i] / factor;
   return res;
}

template <int N>
float dot(vec<N> v, vec<N> w)
{
   float res = 0;
   range (i, N) res += v[i] * w[i];
   return res;
}

template <int N>
float length(vec<N> v)
{
   return sqrt(dot(v, v));
}

template <int N>
vec<N> normalize(vec<N> v)
{
   return v / length(v);
}

template <int N>
vec<N> lerp(vec<N> v, vec<N> w, float t)
{
   return v + (w - v) * t;
}

using vec2 = vec<2>;
using vec3 = vec<3>;
using vec4 = vec<4>;

bool eq(float x, float y)
{
   return abs(x - y) <= max(0.0001f, 0.0001f * max(abs(x), abs(y)));
}

template <int N>
bool eq(vec<N> v, vec<N> w)
{
   range (i, N)
      if (!eq(v[i], w[i]))
         return false;
   return true;
}

vec3 cross(vec3 v, vec3 w)
{
   return {
       v[1] * w[2] - v[2] * w[1],
       v[2] * w[0] - v[0] * w[2],
       v[0] * w[1] - v[1] * w[0]};
}

template <int N, int M>
struct mat
{
   vec<M> data[N];

   vec<M>& operator[](int i)
   {
      return data[i];
   }
};

using mat4 = mat<4, 4>;
using mat3x4 = mat<3, 4>;
using mat3 = mat<3, 3>;
using mat3x2 = mat<3, 2>;
using mat2x3 = mat<2, 3>;
using mat2 = mat<2, 2>;

template <int N, int M>
mat<N, M> operator-(mat<N, M> A)
{
   mat<N, M> res;
   range (i, N)
      range (j, M)
         res[i][j] = -A[i][j];

   return res;
}

template <int N, int M>
vec<N> operator*(mat<N, M> A, vec<M> v)
{
   vec<N> res = {};
   range (i, N)
      range (j, M)
         res[i] += A[i][j] * v[j];
   return res;
}
template <int N, int M, int P>
mat<N, P> operator*(mat<N, M> A, mat<M, P> B)
{
   mat<N, P> res = {};
   range (i, N)
      range (j, P)
         range (k, M)
            res[i][j] += A[i][k] * B[k][j];
   return res;
}

template <int N, int M>
mat<N, M> operator/(mat<N, M> A, float factor)
{
   mat<N, M> res;
   range (i, N)
      range (j, M)
         res[i][j] = A[i][j] / factor;
   return res;
}

template <int N, int M>
mat<M, N> transpose(mat<N, M> A)
{
   mat<M, N> res;
   range (i, N)
      range (j, M)
         res[j][i] = A[i][j];
   return res;
}

mat2 inverse(mat2 A)
{
   float det = A[0][0] * A[1][1] - A[0][1] * A[1][0];
   return mat2{
              A[1][1], -A[0][1],
              -A[1][0], A[0][0]} /
          det;
}

// https://math.stackexchange.com/questions/632525/if-a-is-full-column-rank-then-ata-is-always-invertible
// https://www.physicsforums.com/threads/left-and-right-inverses-of-a-non-square-matrix.1001138/
mat2x3 leftInverse(mat3x2 A)
{
   return inverse(transpose(A) * A) * transpose(A);
}

mat3x2 columns(vec3 v, vec3 w)
{
   return {
       v[0], w[0],
       v[1], w[1],
       v[2], w[2]};
}

mat3 euler(float y, float p, float r)
{
   return {sin(p) * sin(r) * sin(y) + cos(r) * cos(y), sin(p) * sin(y) * cos(r) - sin(r) * cos(y), sin(y) * cos(p),
           sin(r) * cos(p), cos(p) * cos(r), -sin(p),
           sin(p) * sin(r) * cos(y) - sin(y) * cos(r), sin(p) * cos(r) * cos(y) + sin(r) * sin(y), cos(p) * cos(y)};
}

mat3 scale(float x, float y, float z)
{
   return {x, 0, 0,
           0, y, 0,
           0, 0, z};
}

mat4 affine(mat3 A, vec3 v)
{
   return {
       A[0][0], A[0][1], A[0][2], v[0],
       A[1][0], A[1][1], A[1][2], v[1],
       A[2][0], A[2][1], A[2][2], v[2],
       0, 0, 0, 1};
}

template <int N>
mat<N, N> id()
{
   mat<N, N> res = {};
   range (i, N)
      res[i][i] = 1;
   return res;
}
float square(float x)
{
   return x * x;
}

#pragma endregion

#include "shared.h"

struct Texture2D
{
   ComPtr<ID3D11Texture2D> tex;
   ComPtr<ID3D11RenderTargetView> rtv;
   ComPtr<ID3D11DepthStencilView> dsv;
   ComPtr<ID3D11ShaderResourceView> srv;
   operator ID3D11Texture2D*()
   {
      return tex;
   }
   operator ID3D11RenderTargetView*()
   {
      return rtv;
   }
   operator ID3D11DepthStencilView*()
   {
      return dsv;
   }
   operator ID3D11ShaderResourceView*()
   {
      return srv;
   }
};

ComPtr<ID3D11Device> dev;
ComPtr<ID3D11DeviceContext> ctx;
ComPtr<IDXGISwapChain> sc;

void setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY top)
{
   ctx->IASetPrimitiveTopology(top);
}

void clearRenderTargetView(ID3D11RenderTargetView* rtv, vec4 color)
{
   ctx->ClearRenderTargetView(rtv, color.data);
}

void clearDepthStencilView(ID3D11DepthStencilView* dsv, float depth)
{
   ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, depth, 0);
}

void setRenderTargets(vector<ID3D11RenderTargetView*> rtvs, ID3D11DepthStencilView* dsv)
{
   ctx->OMSetRenderTargets(rtvs.size(), rtvs.data(), dsv);
}

void setViewports(vector<D3D11_VIEWPORT> vps)
{
   ctx->RSSetViewports(vps.size(), vps.data());
}

void setVertexConstants(int start, vector<ID3D11Buffer*> cbs)
{
   ctx->VSSetConstantBuffers(start, cbs.size(), cbs.data());
}

void setPixelConstants(int start, vector<ID3D11Buffer*> cbs)
{
   ctx->PSSetConstantBuffers(start, cbs.size(), cbs.data());
}

void setVertexResources(int start, vector<ID3D11ShaderResourceView*> srvs)
{
   ctx->VSSetShaderResources(start, srvs.size(), srvs.data());
}

void setPixelResources(int start, vector<ID3D11ShaderResourceView*> srvs)
{
   ctx->PSSetShaderResources(start, srvs.size(), srvs.data());
}

void setSamplers(int start, vector<ID3D11SamplerState*> samplers)
{
   ctx->PSSetSamplers(start, samplers.size(), samplers.data());
}

void setIndexBuffer(ID3D11Buffer* ib)
{
   ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
}

void setDepthState(ID3D11DepthStencilState* ds)
{
   ctx->OMSetDepthStencilState(ds, 0);
}

void setBlendState(ID3D11BlendState* bs)
{
   ctx->OMSetBlendState(bs, null, 0xffffffff);
}

void setRasterState(ID3D11RasterizerState* rs)
{
   ctx->RSSetState(rs);
}

void setVertexShader(ID3D11VertexShader* vs)
{
   ctx->VSSetShader(vs, null, 0);
}

void setPixelShader(ID3D11PixelShader* ps)
{
   ctx->PSSetShader(ps, null, 0);
}

void draw(int count)
{
   ctx->Draw(count, 0);
}

void drawIndexed(int count)
{
   ctx->DrawIndexed(count, 0, 0);
}

struct Mesh
{
   ComPtr<ID3D11ShaderResourceView> points;
   ComPtr<ID3D11ShaderResourceView> normals;
   ComPtr<ID3D11ShaderResourceView> uvs;
   ComPtr<ID3D11Buffer> indices;
   int count;
   Texture2D diffuse;

   void bind()
   {
      setVertexResources(0, {points, normals, uvs});
      setPixelResources(0, {diffuse});
      setIndexBuffer(indices);
   }

   void draw()
   {
      drawIndexed(count);
   }
};

D3D11_VIEWPORT viewport(float width, float height)
{
   return {
       .Width = width,
       .Height = height,
       .MaxDepth = 1,
   };
}

ComPtr<ID3D11RenderTargetView> createRenderTargetView(ID3D11Texture2D* tex)
{
   ComPtr<ID3D11RenderTargetView> rtv;
   dev->CreateRenderTargetView(tex, null, &rtv);
   return rtv;
}
ComPtr<ID3D11RenderTargetView> createRenderTargetView(ID3D11Texture2D* tex, D3D11_RENDER_TARGET_VIEW_DESC desc)
{
   ComPtr<ID3D11RenderTargetView> rtv;
   dev->CreateRenderTargetView(tex, &desc, &rtv);
   return rtv;
}

ComPtr<ID3D11DepthStencilView> createDepthStencilView(ID3D11Texture2D* tex)
{
   ComPtr<ID3D11DepthStencilView> dsv;
   dev->CreateDepthStencilView(tex, null, &dsv);
   return dsv;
}

ComPtr<ID3D11DepthStencilView> createDepthStencilView(ID3D11Texture2D* tex, D3D11_DEPTH_STENCIL_VIEW_DESC desc)
{
   ComPtr<ID3D11DepthStencilView> dsv;
   dev->CreateDepthStencilView(tex, &desc, &dsv);
   return dsv;
}

ComPtr<ID3D11ShaderResourceView> createShaderResourceView(ID3D11Resource* res)
{
   ComPtr<ID3D11ShaderResourceView> srv;
   dev->CreateShaderResourceView(res, null, &srv);
   return srv;
}

ComPtr<ID3D11ShaderResourceView> createShaderResourceView(ID3D11Resource* res, D3D11_SHADER_RESOURCE_VIEW_DESC desc)
{
   ComPtr<ID3D11ShaderResourceView> srv;
   dev->CreateShaderResourceView(res, &desc, &srv);
   return srv;
}

ComPtr<ID3D11Buffer> createBuffer(D3D11_BUFFER_DESC desc)
{
   ComPtr<ID3D11Buffer> buf;
   dev->CreateBuffer(&desc, null, &buf);
   return buf;
}

ComPtr<ID3D11Buffer> createBuffer(D3D11_BUFFER_DESC desc, D3D11_SUBRESOURCE_DATA data)
{
   ComPtr<ID3D11Buffer> buf;
   dev->CreateBuffer(&desc, &data, &buf);
   return buf;
}

void* mapResource(ID3D11Resource* res)
{
   D3D11_MAPPED_SUBRESOURCE mapped;
   ctx->Map(res, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
   return mapped.pData;
}

void unmapResource(ID3D11Resource* res)
{
   ctx->Unmap(res, 0);
}

Texture2D createTexture(D3D11_TEXTURE2D_DESC desc)
{

   if (desc.MipLevels == 0)
      desc.MipLevels = 1;
   if (desc.ArraySize == 0)
      desc.ArraySize = 1;
   if (desc.SampleDesc.Count == 0)
      desc.SampleDesc.Count = 1;

   Texture2D tex;
   dev->CreateTexture2D(&desc, null, &tex.tex);

   if (desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      tex.rtv = createRenderTargetView(tex);

   if (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
   {
      if (desc.Format == DXGI_FORMAT_R32_TYPELESS)
         tex.dsv = createDepthStencilView(tex, {
                                                   .Format = DXGI_FORMAT_D32_FLOAT,
                                                   .ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D,
                                               });
      else
         tex.dsv = createDepthStencilView(tex);
   }

   if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
   {

      if (desc.Format == DXGI_FORMAT_R32_TYPELESS)
         tex.srv = createShaderResourceView(tex, {
                                                     .Format = DXGI_FORMAT_R32_FLOAT,
                                                     .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
                                                     .Texture2D = {.MipLevels = desc.MipLevels},
                                                 });
      else
         tex.srv = createShaderResourceView(tex);
   }
   return tex;
}

int formatSize(DXGI_FORMAT format)
{
   switch (format)
   {
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
         return 4;

      default:
         throw;
   }
}

Texture2D createTexture(D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA data)
{
   if (desc.MipLevels == 0)
      desc.MipLevels = 1;
   if (desc.ArraySize == 0)
      desc.ArraySize = 1;
   if (desc.SampleDesc.Count == 0)
      desc.SampleDesc.Count = 1;

   if (desc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
      desc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

   if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
      data.SysMemPitch = desc.Width * 4;

   Texture2D tex;
   dev->CreateTexture2D(&desc, vector<D3D11_SUBRESOURCE_DATA>(desc.MipLevels * desc.ArraySize, data).data(), &tex.tex);

   if (desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      tex.rtv = createRenderTargetView(tex);

   if (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      tex.dsv = createDepthStencilView(tex);

   if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      tex.srv = createShaderResourceView(tex);

   if (desc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
      ctx->GenerateMips(tex);
   return tex;
}

ComPtr<ID3DBlob> readBlob(wstring path)
{
   ComPtr<ID3DBlob> blob;
   D3DReadFileToBlob(path.data(), &blob);
   return blob;
}

ComPtr<ID3D11VertexShader> createVertexShader(wstring path)
{
   auto blob = readBlob(path);
   ComPtr<ID3D11VertexShader> vs;
   dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), null, &vs);
   return vs;
}

ComPtr<ID3D11PixelShader> createPixelShader(wstring path)
{
   auto blob = readBlob(path);
   ComPtr<ID3D11PixelShader> ps;
   dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), null, &ps);
   return ps;
}

ComPtr<ID3D11SamplerState> createSamplerState(D3D11_SAMPLER_DESC desc)
{
   ComPtr<ID3D11SamplerState> ss;
   dev->CreateSamplerState(&desc, &ss);
   return ss;
}

ComPtr<ID3D11DepthStencilState> createDepthState(D3D11_DEPTH_STENCIL_DESC desc)
{
   ComPtr<ID3D11DepthStencilState> ds;
   dev->CreateDepthStencilState(&desc, &ds);
   return ds;
}

ComPtr<ID3D11RasterizerState> createRasterState(D3D11_RASTERIZER_DESC desc)
{
   ComPtr<ID3D11RasterizerState> rs;
   dev->CreateRasterizerState(&desc, &rs);
   return rs;
}

ComPtr<ID3D11BlendState> createBlendState(D3D11_BLEND_DESC desc)
{
   ComPtr<ID3D11BlendState> bs;
   dev->CreateBlendState(&desc, &bs);
   return bs;
}

void loadMesh(tinygltf::Model scene, Mesh* mesh, int index)
{
}

template <class T>
struct cb
{
   ComPtr<ID3D11Buffer> buf;

   operator ID3D11Buffer*()
   {
      return buf;
   }

   cb()
   {
      buf = createBuffer({
          .ByteWidth = sizeof(T) + 15 & ~15,
          .Usage = D3D11_USAGE_DYNAMIC,
          .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
          .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
      });
   }

   void operator=(T data)
   {
      D3D11_MAPPED_SUBRESOURCE mapped;
      ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
      *(T*)mapped.pData = data;
      ctx->Unmap(buf, 0);
   }
};