#include "header.h"

bool keyDown[256];
bool dragging;

struct
{
   vec3 pos = {0, 0, -5};
   float yaw;
   float pitch;
   float armLength;
   float fov = 103 * pi / 180;
   float near = 0.1;
   float sens = 0.003;
   float speed = 5;
} cam;

struct
{
   float yaw = -pi / 6;
   float pitch = pi / 3;
} shadow;

i64 callback(HWND hwnd, u32 msg, u64 wp, i64 lp)
{
   switch (msg)
   {
      case WM_KEYDOWN:
         if (!(lp >> 30 & 1)) // not repeated
         {
            u8 key = wp;
            keyDown[key] = true;
            switch (key)
            {
               case VK_ESCAPE:
                  ExitProcess(0);
                  break;
            }
         }
         break;

      case WM_KEYUP:
      {
         u8 key = wp;
         keyDown[key] = false;
      }
      break;

      case WM_DESTROY:
         ExitProcess(0);
         break;

      case WM_INPUT:
      {
         RAWINPUT raw;
         u32 size = sizeof(raw);
         GetRawInputData(HRAWINPUT(lp), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

         //  note: +x is right, +y is down
         //  println("{} {}", raw.data.mouse.lLastX, raw.data.mouse.lLastY);
         cam.pitch = clamp(cam.pitch + raw.data.mouse.lLastY * cam.sens, -pi / 2, pi / 2);
         cam.yaw = fmod(cam.yaw + raw.data.mouse.lLastX * cam.sens, 2 * pi);
      }
      break;

         // case WM_SETCURSOR: // https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor
         // if (LOWORD(lp) == HTCLIENT)
         // { // in main client area
         //   // if (dragging)
         //    SetCursor(LoadCursor(null, IDC_SIZEALL));
         //    // else
         //    // SetCursor(LoadCursor(null, IDC_ARROW));
         // }
         // return 1;

      default:
         return DefWindowProc(hwnd, msg, wp, lp);
   }

   return 0;
}

// int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
int main()
{

   vector<RAWINPUTDEVICE> devices = {
       {
           .usUsagePage = HID_USAGE_PAGE_GENERIC,
           .usUsage = HID_USAGE_GENERIC_MOUSE,
       }};

   RegisterRawInputDevices(devices.data(), devices.size(), sizeof(RAWINPUTDEVICE));
   SetProcessDPIAware();

   RegisterClass(&WNDCLASS{
       .lpfnWndProc = callback,
       .hCursor = LoadCursor(null, IDC_ARROW),
       .lpszClassName = "Game",
   });

   auto hwnd = CreateWindow("Game", "Game",
                            WS_POPUP | WS_MAXIMIZE, 0, 0, 0, 0,
                            // WS_OVERLAPPEDWINDOW ^ (WS_SIZEBOX | WS_MAXIMIZEBOX), 50, 50, 1920, 1080,
                            null, null, null, null);

   RECT rect;
   GetClientRect(hwnd, &rect);
   vec2 windowSize = {rect.right, rect.bottom};

   D3D11CreateDeviceAndSwapChain(
       null, D3D_DRIVER_TYPE_HARDWARE, null,
#ifdef _DEBUG
       D3D11_CREATE_DEVICE_DEBUG,
#else
       0,
#endif
       null, 0, D3D11_SDK_VERSION,
       &DXGI_SWAP_CHAIN_DESC{
           .BufferDesc = {.Format = DXGI_FORMAT_R8G8B8A8_UNORM},
           .SampleDesc = {.Count = 1},
           .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
           .BufferCount = 2,
           .OutputWindow = hwnd,
           .Windowed = true,
           .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
       },
       &sc, &dev, null, &ctx);

#ifdef _DEBUG
   ComPtr<ID3D11InfoQueue> info;
   dev.As(&info);
   info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
   info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
#endif

   Texture2D texWindow;
   sc->GetBuffer(0, IID_PPV_ARGS(&texWindow.tex));
   texWindow.rtv = createRenderTargetView(texWindow, {.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                                      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D});
   // texWindow.rtv = createRenderTargetView(texWindow);

   auto depthWindow = createTexture({
       .Width = u32(windowSize[0]),
       .Height = u32(windowSize[1]),
       .Format = DXGI_FORMAT_D32_FLOAT,
       .BindFlags = D3D11_BIND_DEPTH_STENCIL,
   });

   auto spriteBatch = SpriteBatch(ctx);
   auto spriteFont = SpriteFont(dev, L"assets/inter.spritefont");

   auto ssAniso = createSamplerState({
       .Filter = D3D11_FILTER_ANISOTROPIC,
       .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
       .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
       .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
       .MaxAnisotropy = 16,
       .MinLOD = -FLT_MAX,
       .MaxLOD = FLT_MAX,
   });
   auto ssLinear = createSamplerState({
       .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
       .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
       .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
       .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
       .MinLOD = -FLT_MAX,
       .MaxLOD = FLT_MAX,
   });
   auto ssPoint = createSamplerState({
       .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
       .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
       .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
       .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
   });
   auto ssCmp = createSamplerState({
       .Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
       .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
       .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
       .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
       .ComparisonFunc = D3D11_COMPARISON_LESS,
   });

   auto dsGreater = createDepthState({
       .DepthEnable = true,
       .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
       .DepthFunc = D3D11_COMPARISON_GREATER,
   });

   auto dsLess = createDepthState({
       .DepthEnable = true,
       .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
       .DepthFunc = D3D11_COMPARISON_LESS,
   });

   auto bsOpaque = createBlendState({.RenderTarget = {{
                                         .BlendEnable = false,
                                         .SrcBlend = D3D11_BLEND_ONE,
                                         .DestBlend = D3D11_BLEND_ZERO,
                                         .BlendOp = D3D11_BLEND_OP_ADD,
                                         .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
                                     }}});

   LARGE_INTEGER prev, freq;
   QueryPerformanceCounter(&prev);
   QueryPerformanceFrequency(&freq);
   ShowWindow(hwnd, SW_SHOW);
   // ShowCursor(false);

   TinyGLTF loader;
   Model scene;
   loader.LoadBinaryFromFile(&scene, null, null, "scene.glb");

   struct
   {
      ComPtr<ID3D11ShaderResourceView> points;
      ComPtr<ID3D11ShaderResourceView> normals;
      ComPtr<ID3D11ShaderResourceView> uvs;
      ComPtr<ID3D11Buffer> indices;
      int count;
      Texture2D diffuse;
   } player, enemy, level;

   enemy.points = createShaderResourceView(createBuffer({
                                                            .ByteWidth = LEN(scene, 0, attributes["POSITION"]),
                                                            .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                                                            .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
                                                            .StructureByteStride = sizeof(vec3),
                                                        },
                                                        {
                                                            .pSysMem = DATA(scene, 0, attributes["POSITION"]),
                                                        }));

   enemy.normals = createShaderResourceView(createBuffer({
                                                             .ByteWidth = LEN(scene, 0, attributes["NORMAL"]),
                                                             .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                                                             .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
                                                             .StructureByteStride = sizeof(vec3),
                                                         },
                                                         {
                                                             .pSysMem = DATA(scene, 0, attributes["NORMAL"]),
                                                         }));

   enemy.indices = createBuffer({
                                    .ByteWidth = LEN(scene, 0, indices),
                                    .BindFlags = D3D11_BIND_INDEX_BUFFER,
                                },
                                {
                                    .pSysMem = DATA(scene, 0, indices),
                                });
   enemy.count = COUNT(scene, 0, indices);

   player.points = createShaderResourceView(createBuffer({
                                                             .ByteWidth = LEN(scene, 1, attributes["POSITION"]),
                                                             .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                                                             .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
                                                             .StructureByteStride = sizeof(vec3),
                                                         },
                                                         {
                                                             .pSysMem = DATA(scene, 1, attributes["POSITION"]),
                                                         }));

   player.normals = createShaderResourceView(createBuffer({
                                                              .ByteWidth = LEN(scene, 1, attributes["NORMAL"]),
                                                              .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                                                              .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
                                                              .StructureByteStride = sizeof(vec3),
                                                          },
                                                          {
                                                              .pSysMem = DATA(scene, 1, attributes["NORMAL"]),
                                                          }));

   player.uvs = createShaderResourceView(createBuffer({
                                                          .ByteWidth = LEN(scene, 1, attributes["TEXCOORD_0"]),
                                                          .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                                                          .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
                                                          .StructureByteStride = sizeof(vec2),
                                                      },
                                                      {
                                                          .pSysMem = DATA(scene, 1, attributes["TEXCOORD_0"]),
                                                      }));

   player.indices = createBuffer({
                                     .ByteWidth = LEN(scene, 1, indices),
                                     .BindFlags = D3D11_BIND_INDEX_BUFFER,
                                 },
                                 {
                                     .pSysMem = DATA(scene, 1, indices),
                                 });
   player.count = COUNT(scene, 1, indices);

   player.diffuse = createTexture({
                                      .Width = u32(scene.images[1].width),
                                      .Height = u32(scene.images[1].height),
                                      .MipLevels = BIT_WIDTH(scene.images[1].width),
                                      .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                      .MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS,
                                  },
                                  {
                                      .pSysMem = scene.images[1].image.data(),
                                  });

   level.points = createShaderResourceView(createBuffer({
                                                            .ByteWidth = LEN(scene, 2, attributes["POSITION"]),
                                                            .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                                                            .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
                                                            .StructureByteStride = sizeof(vec3),
                                                        },
                                                        {
                                                            .pSysMem = DATA(scene, 2, attributes["POSITION"]),
                                                        }));

   level.normals = createShaderResourceView(createBuffer({
                                                             .ByteWidth = LEN(scene, 2, attributes["NORMAL"]),
                                                             .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                                                             .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
                                                             .StructureByteStride = sizeof(vec3),
                                                         },
                                                         {
                                                             .pSysMem = DATA(scene, 2, attributes["NORMAL"]),
                                                         }));

   auto vsTriangle = createVertexShader(L"out/vsTriangle.dxbc");
   auto psTriangle = createPixelShader(L"out/psTriangle.dxbc");
   auto vsMain = createVertexShader(L"out/vsMain.dxbc");
   auto psMain = createPixelShader(L"out/psMain.dxbc");

   cb<mat4> model, view, proj, cascades[4]; // cascades transform world space to shadow
   cb<vec3> light;

   light = {-sin(shadow.yaw) * cos(shadow.pitch), sin(shadow.pitch), -cos(shadow.pitch) * cos(shadow.yaw)};

   print("hello world");

   while (true)
   {
      MSG msg;
      while (PeekMessage(&msg, null, 0, 0, PM_REMOVE))
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }

      LARGE_INTEGER now;
      QueryPerformanceCounter(&now);
      float dt = float(now.QuadPart - prev.QuadPart) / freq.QuadPart;

      prev = now;

      mat3 viewToWorldRotation = euler(cam.yaw, cam.pitch, 0);

      vec3 camRight = viewToWorldRotation * vec3{1, 0, 0};
      vec3 camUp = viewToWorldRotation * vec3{0, 1, 0};
      vec3 camForward = viewToWorldRotation * vec3{0, 0, 1};

      if (keyDown['W'])
         cam.pos += camForward * cam.speed * dt;
      if (keyDown['S'])
         cam.pos -= camForward * cam.speed * dt;
      if (keyDown['A'])
         cam.pos -= camRight * cam.speed * dt;
      if (keyDown['D'])
         cam.pos += camRight * cam.speed * dt;

      setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      clearRenderTargetView(texWindow, {0, 0, 0, 1});
      clearDepthStencilView(depthWindow, 0); // reverse depth means 0 = far plane
                                             // so error during world space -> view space -> clip space -> screen space
                                             // conversion is spread out more evenly (projection matrix defines a
                                             // function z |-> near/z)
      setRenderTargets({texWindow}, depthWindow);
      setViewports({{.Width = windowSize[0], .Height = windowSize[1], .MaxDepth = 1}});
      setVertexConstants(0, {model, view, proj});
      setPixelConstants(0, {light});
      setDepthState(dsGreater);
      setBlendState(bsOpaque);

      float aspect = windowSize[0] / windowSize[1];

      view =
          //   {
          //       cos(cam.yaw), 0, -sin(cam.yaw), -dot(cam.pos, camRight),
          //       sin(cam.pitch) * sin(cam.yaw), cos(cam.pitch), sin(cam.pitch) * cos(cam.yaw), -dot(cam.pos, camUp),
          //       sin(cam.yaw) * cos(cam.pitch), -sin(cam.pitch), cos(cam.pitch) * cos(cam.yaw), -dot(cam.pos, camForward),
          //       0, 0, 0, 1};

          affine(transpose(viewToWorldRotation), vec3{}) * affine(id<3>(), -cam.pos);
      proj = {
          1 / tan(cam.fov / 2), 0, 0, 0,
          0, aspect / tan(cam.fov / 2), 0, 0,
          0, 0, 0, cam.near,
          0, 0, 1, 0};
      model = {
          1, 0, 0, 0,
          0, 1, 0, 0,
          0, 0, 1, 0,
          0, 0, 0, 1};

      //   range (i, 8)
      //   {
      //   }

      //   setVertexShader(vsTriangle);
      //   setPixelShader(psTriangle);
      //   draw(3);

      setVertexShader(vsMain);
      setPixelShader(psMain);

      setVertexResources(0, {player.points, player.normals, player.uvs});
      setPixelResources(0, {player.diffuse});
      setIndexBuffer(player.indices);
      drawIndexed(player.count);

      spriteBatch.Begin();
      spriteFont.DrawString(&spriteBatch, L"Hello, world!", Vector2(100, 100));
      auto width = spriteFont.MeasureString(L"Hello, world!");
      spriteBatch.End();

      sc->Present(1, 0);
   }

   return 0;
}