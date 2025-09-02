#include "header.h"

bool keyDown[256];
bool dragging;

struct
{
   vec3 pos = {0, 5, -5};
   float yaw;
   float pitch;
   float armLength;
   float fov = 103 * pi / 180;
   float near = 0.1;
   float sens = 0.003;
   float speed = 5;
} cam;

Mesh enemy, level;

struct : Mesh
{
   vec3 pos;
} player;

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

#pragma region setup
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
#pragma endregion

   Texture2D texWindow;
   sc->GetBuffer(0, IID_PPV_ARGS(&texWindow.tex));
   texWindow.rtv = createRenderTargetView(texWindow, {.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                                      .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D});

   vec2 windowSize = {rect.right, rect.bottom};

   auto depthWindow = createTexture({
       .Width = u32(windowSize[0]),
       .Height = u32(windowSize[1]),
       .Format = DXGI_FORMAT_D32_FLOAT,
       .BindFlags = D3D11_BIND_DEPTH_STENCIL,
   });

   //    material id so that we shade the level with ssao and the player with half lambert lighting
   //    auto texMaterial = createTexture();

   Texture2D shadowMap[4];
   range (i, 4)
      shadowMap[i] = createTexture({
          .Width = 2048,
          .Height = 2048,
          .Format = DXGI_FORMAT_R32_TYPELESS,
          .BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
      });

   auto spriteBatch = SpriteBatch(ctx);
   auto spriteFont = SpriteFont(dev, L"assets/inter.spritefont");

#pragma region states
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
   auto ssGrid = createSamplerState({
       .Filter = D3D11_FILTER_ANISOTROPIC,
       .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
       .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
       .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
       .MaxAnisotropy = 16,
       .MinLOD = -FLT_MAX,
       .MaxLOD = FLT_MAX,
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

   auto rsFill = createRasterState({
       .FillMode = D3D11_FILL_SOLID,
       .CullMode = D3D11_CULL_BACK,
       .DepthClipEnable = true,
   });
   auto rsShadow = createRasterState({
       .FillMode = D3D11_FILL_SOLID,
       .CullMode = D3D11_CULL_BACK,
       .DepthBiasClamp = 0.05,
       .SlopeScaledDepthBias = 3,
       // depth clip false to pancake depth
   });

#pragma endregion

   tinygltf::TinyGLTF loader;
   tinygltf::Model scene;
   loader.LoadBinaryFromFile(&scene, null, null, "scene.glb");

#pragma region enemy
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

#pragma endregion

#pragma region player
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
   player.pos = {
       float(scene.nodes[1].translation[0]),
       float(scene.nodes[1].translation[1]),
       float(scene.nodes[1].translation[2])};
#pragma endregion

#pragma region level
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
   level.indices = createBuffer({
                                    .ByteWidth = LEN(scene, 2, indices),
                                    .BindFlags = D3D11_BIND_INDEX_BUFFER,
                                },
                                {
                                    .pSysMem = DATA(scene, 2, indices),
                                });

   level.count = COUNT(scene, 2, indices);

   level.diffuse = createTexture({
                                     .Width = u32(scene.images[2].width),
                                     .Height = u32(scene.images[2].height),
                                     .MipLevels = BIT_WIDTH(scene.images[2].width),
                                     .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                     .MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS,
                                 },
                                 {
                                     .pSysMem = scene.images[2].image.data(),
                                 });
#pragma endregion

   auto vsTriangle = createVertexShader(L"out/vsTriangle.dxbc");
   auto vsShadow = createVertexShader(L"out/vsShadow.dxbc");
   auto vsMain = createVertexShader(L"out/vsMain.dxbc");
   auto vsTriplanar = createVertexShader(L"out/vsTriplanar.dxbc");

   auto psTriangle = createPixelShader(L"out/psTriangle.dxbc");
   auto psMain = createPixelShader(L"out/psMain.dxbc");

   cb<mat4> model, view, proj, cascades[4]; // cascades transform world space to shadow
   cb<vec3> light;

   mat3 shadowToWorldRotation = euler(-pi / 6, pi / 3, 0);
   light = -shadowToWorldRotation * vec3{0, 0, 1};

   print("hello world");

   LARGE_INTEGER prev, freq;
   QueryPerformanceCounter(&prev);
   QueryPerformanceFrequency(&freq);
   ShowWindow(hwnd, SW_SHOW);
   // ShowCursor(false);
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

      float aspect = windowSize[0] / windowSize[1];
      view = affine(transpose(viewToWorldRotation), vec3{}) * affine(id<3>(), -cam.pos);
      proj = {
          1 / tan(cam.fov / 2), 0, 0, 0,
          0, aspect / tan(cam.fov / 2), 0, 0,
          0, 0, 0, cam.near,
          0, 0, 1, 0};

#pragma region update cascades

      float d[] = {cam.near, 4, 10, 20, 50};
      range (i, 4)
      {
         float x1 = d[i] * tan(cam.fov / 2);
         float y1 = x1 / aspect;
         float x2 = d[i + 1] * tan(cam.fov / 2);
         float y2 = x2 / aspect;
         vec3 frustumPoints[] = {
             {x1, -y1, d[i]},
             {x1, y1, d[i]},
             {-x1, y1, d[i]},
             {-x1, -y1, d[i]},
             {x2, -y2, d[i + 1]},
             {x2, y2, d[i + 1]},
             {-x2, y2, d[i + 1]},
             {-x2, -y2, d[i + 1]},
         };
         float xMin, yMin, zMin, xMax, yMax, zMax;
         xMin = yMin = zMin = FLT_MAX;
         xMax = yMax = zMax = -FLT_MAX;
         range (i, 8)
         {
            vec3 v = transpose(shadowToWorldRotation) * (viewToWorldRotation * frustumPoints[i] + cam.pos);
            xMin = min(xMin, v[0]);
            yMin = min(yMin, v[1]);
            zMin = min(zMin, v[2]);
            xMax = max(xMax, v[0]);
            yMax = max(yMax, v[1]);
            zMax = max(zMax, v[2]);
         }
         // minimize floating point error by making s an integer
         float s = ceil(max(length(frustumPoints[0] - frustumPoints[6]), length(frustumPoints[4] - frustumPoints[6])));
         float T = s / 2048;

         // divide by s/2 to keep x, y in [-1, 1]
         // you can imagine an s x s square in world space where rasterization takes place.
         // normally you subtract a = (xMin + xMax) / 2.
         // instead you can keep region aligned to integer multiples of s / 2048 x s / 2048 so that it's rasterized the same way
         // by first aligning a to a bottom-left point and then subtracting that.
         // in effect, you are shifting the (xMax - xMin) x (yMax - yMin) rectangle to the up and right by a value in [0, s / 2048)
         cascades[i] = affine(scale(2 / s, 2 / s, 1 / (zMax - zMin)), {0, 0, 0}) *
                       affine(transpose(shadowToWorldRotation), {-floor((xMin + xMax) / 2 / T) * T, -floor((yMin + yMax) / 2 / T) * T, -zMin});
         //    affine(transpose(shadowToWorldRotation), {-(xMin + xMax) / 2, -(yMin + yMax) / 2, -zMin});
      }

#pragma endregion

      setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      setBlendState(null);

      setDepthState(dsLess);
      setRasterState(rsShadow);

      setVertexShader(vsShadow);
      setPixelShader(null);

      range (i, 4)
      {
         setVertexConstants(0, {model, cascades[i]});
         clearDepthStencilView(shadowMap[i], 1);
         setRenderTargets({}, shadowMap[i]);
         model = id<4>();
         level.bind();
         level.draw();
      }

      setVertexConstants(0, {model, view, proj});
      setPixelConstants(0, {light});

      clearRenderTargetView(texWindow, rgba(148, 178, 213, 1));
      clearDepthStencilView(depthWindow, 0); // reverse depth means 0 = far plane
                                             // so error during world space -> view space -> clip space -> screen space
                                             // conversion is spread out more evenly (projection matrix defines a
                                             // function z |-> near/z)
      setRenderTargets({texWindow}, depthWindow);
      setViewports({viewport(windowSize[0], windowSize[1])});

      setDepthState(dsGreater);
      setRasterState(rsFill);

      setSamplers(0, {ssGrid});

      setVertexShader(vsTriplanar);
      setPixelShader(psMain);

      model = id<4>();
      level.bind();
      level.draw();

      setVertexShader(vsMain);
      model = affine(id<3>(), player.pos);
      player.bind();
      player.draw();

      spriteBatch.Begin();
      spriteFont.DrawString(&spriteBatch, L"Hello, world!", Vector2(100, 100));
      auto width = spriteFont.MeasureString(L"Hello, world!");
      spriteBatch.End();

      sc->Present(1, 0);
   }

   return 0;
}