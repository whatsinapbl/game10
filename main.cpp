// #include "header.h"

// bool keyDown[256];

// struct
// {
//    vec3 pos = {0, 5, -5};
//    float yaw;
//    float pitch;
//    float armLength = 8;
//    float fov = 103 * pi / 180;
//    float near = 0.1;
//    float sens = 0.003;
//    float speed = 5;
// } cam;

// Mesh enemy, level;

// struct Player : Mesh
// {
//    vec3 curPos, prevPos, curVel;
//    float yaw;
//    enum
//    {
//       Ground,
//       Air
//    } state = Air;
//    float innerRadius = 0.45;
//    float outerRadius = 0.5;
// } player;

// struct
// {
//    float rate = 64;
//    float elapsed;
// } physics;

// i64 callback(HWND hwnd, u32 msg, u64 wp, i64 lp)
// {
//    switch (msg)
//    {
//       case WM_MOUSEWHEEL:
//          cam.speed *= pow(2, float(GET_WHEEL_DELTA_WPARAM(wp)) / WHEEL_DELTA);
//          break;
//       case WM_KEYDOWN:
//          if (!(lp >> 30 & 1)) // not repeated
//          {
//             u8 key = wp;
//             keyDown[key] = true;
//             switch (key)
//             {
//                case VK_ESCAPE:
//                   ExitProcess(0);
//                   break;
//             }
//          }
//          break;

//       case WM_KEYUP:
//       {
//          u8 key = wp;
//          keyDown[key] = false;
//       }
//       break;

//       case WM_DESTROY:
//          ExitProcess(0);
//          break;

//       case WM_INPUT:
//       {
//          RAWINPUT raw;
//          u32 size = sizeof(raw);
//          GetRawInputData(HRAWINPUT(lp), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));

//          //  note: +x is right, +y is down
//          //  println("{} {}", raw.data.mouse.lLastX, raw.data.mouse.lLastY);
//          cam.pitch = clamp(cam.pitch + raw.data.mouse.lLastY * cam.sens, -pi / 2, pi / 2);
//          cam.yaw = fmod(cam.yaw + raw.data.mouse.lLastX * cam.sens, 2 * pi);
//       }
//       break;

//          // case WM_SETCURSOR: // https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor
//          // if (LOWORD(lp) == HTCLIENT)
//          // { // in main client area
//          //   // if (dragging)
//          //    SetCursor(LoadCursor(null, IDC_SIZEALL));
//          //    // else
//          //    // SetCursor(LoadCursor(null, IDC_ARROW));
//          // }
//          // return 1;

//       default:
//          return DefWindowProc(hwnd, msg, wp, lp);
//    }

//    return 0;
// }

// int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
// int main()
// {

//    // vector<int> v = {1};
//    // for (int i = 0; i < v.size(); i++)
//    //    v.push_back(2);
//    float x = pow(0, 0);
//    // double y = pow(0.0, 0.0);
//    float f = NAN;
//    set<float> s;
//    s.insert(3);
//    s.insert(NAN);
//    for (auto x : s)
//    {
//       println("{}", x);
//    }
//    println("{}", s.size());

// #pragma region setup
//    vector<RAWINPUTDEVICE> devices = {
//        {
//            .usUsagePage = HID_USAGE_PAGE_GENERIC,
//            .usUsage = HID_USAGE_GENERIC_MOUSE,
//        }};

//    RegisterRawInputDevices(devices.data(), devices.size(), sizeof(RAWINPUTDEVICE));
//    SetProcessDPIAware();

//    RegisterClass(&WNDCLASS{
//        .lpfnWndProc = callback,
//        .hCursor = LoadCursor(null, IDC_ARROW),
//        .lpszClassName = "Game",
//    });

//    auto hwnd = CreateWindow("Game", "Game",
//                             WS_POPUP | WS_MAXIMIZE, 0, 0, 0, 0,
//                             // WS_OVERLAPPEDWINDOW ^ (WS_SIZEBOX | WS_MAXIMIZEBOX), 50, 50, 1920, 1080,
//                             null, null, null, null);

//    RECT rect;
//    GetClientRect(hwnd, &rect);

//    D3D11CreateDeviceAndSwapChain(
//        null, D3D_DRIVER_TYPE_HARDWARE, null,
// #ifdef _DEBUG
//        D3D11_CREATE_DEVICE_DEBUG,
// #else
//        0,
// #endif
//        null, 0, D3D11_SDK_VERSION,
//        &DXGI_SWAP_CHAIN_DESC{
//            .BufferDesc = {.Format = DXGI_FORMAT_R8G8B8A8_UNORM},
//            .SampleDesc = {.Count = 1},
//            .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
//            .BufferCount = 2,
//            .OutputWindow = hwnd,
//            .Windowed = true,
//            .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
//        },
//        &sc, &dev, null, &ctx);

// #ifdef _DEBUG
//    ComPtr<ID3D11InfoQueue> info;
//    dev.As(&info);
//    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
//    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
// //    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
// #endif
// #pragma endregion

//    Texture2D texWindow;
//    sc->GetBuffer(0, IID_PPV_ARGS(&texWindow.tex));
//    texWindow.rtv = createRenderTargetView(texWindow, {.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
//                                                       .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D});

//    vec2 windowSize = {rect.right, rect.bottom};

//    auto depthWindow = createTexture({
//        .Width = u32(windowSize[0]),
//        .Height = u32(windowSize[1]),
//        .Format = DXGI_FORMAT_D32_FLOAT,
//        .BindFlags = D3D11_BIND_DEPTH_STENCIL,
//    });

//    //    material id so that we shade the level with ssao and the player with half lambert lighting
//    //    auto texMaterial = createTexture();

//    Texture2D shadowMap[4];
//    range (i, 4)
//       shadowMap[i] = createTexture({
//           .Width = 2048,
//           .Height = 2048,
//           .Format = DXGI_FORMAT_R32_TYPELESS,
//           .BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE,
//       });

//    auto spriteBatch = SpriteBatch(ctx);
//    auto spriteFont = SpriteFont(dev, L"inter.spritefont");

// #pragma region states
//    auto ssGrid = createSamplerState({
//        .Filter = D3D11_FILTER_ANISOTROPIC,
//        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
//        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
//        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
//        .MaxAnisotropy = 16,
//        .MinLOD = -FLT_MAX,
//        .MaxLOD = FLT_MAX,
//    });
//    auto ssLinear = createSamplerState({
//        .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
//        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
//        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
//        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
//        .MinLOD = -FLT_MAX,
//        .MaxLOD = FLT_MAX,
//    });
//    auto ssPoint = createSamplerState({
//        .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
//        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
//        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
//        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
//    });
//    auto ssCmp = createSamplerState({
//        .Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
//        .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
//        .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
//        .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
//        .ComparisonFunc = D3D11_COMPARISON_LESS,
//        // minlod = maxlod = 0 (no mips)
//    });

//    auto dsGreater = createDepthState({
//        .DepthEnable = true,
//        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
//        .DepthFunc = D3D11_COMPARISON_GREATER,
//    });

//    auto dsLess = createDepthState({
//        .DepthEnable = true,
//        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
//        .DepthFunc = D3D11_COMPARISON_LESS,
//    });

//    auto rsFill = createRasterState({
//        .FillMode = D3D11_FILL_SOLID,
//        .CullMode = D3D11_CULL_BACK,
//        .DepthClipEnable = true,
//    });
//    auto rsShadow = createRasterState({
//        .FillMode = D3D11_FILL_SOLID,
//        .CullMode = D3D11_CULL_BACK,
//        .DepthBiasClamp = 0.05,
//        .SlopeScaledDepthBias = 3,
//        // depth clip false to pancake depth
//    });

// #pragma endregion

//    tinygltf::TinyGLTF loader;
//    tinygltf::Model scene;
//    loader.LoadBinaryFromFile(&scene, null, null, "scene.glb");

// #pragma region enemy
//    enemy.points = createShaderResourceView(createBuffer({
//                                                             .ByteWidth = LEN(scene, 0, attributes["POSITION"]),
//                                                             .BindFlags = D3D11_BIND_SHADER_RESOURCE,
//                                                             .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
//                                                             .StructureByteStride = sizeof(vec3),
//                                                         },
//                                                         {
//                                                             .pSysMem = DATA(scene, 0, attributes["POSITION"]),
//                                                         }));

//    enemy.normals = createShaderResourceView(createBuffer({
//                                                              .ByteWidth = LEN(scene, 0, attributes["NORMAL"]),
//                                                              .BindFlags = D3D11_BIND_SHADER_RESOURCE,
//                                                              .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
//                                                              .StructureByteStride = sizeof(vec3),
//                                                          },
//                                                          {
//                                                              .pSysMem = DATA(scene, 0, attributes["NORMAL"]),
//                                                          }));

//    enemy.indices = createBuffer({
//                                     .ByteWidth = LEN(scene, 0, indices),
//                                     .BindFlags = D3D11_BIND_INDEX_BUFFER,
//                                 },
//                                 {
//                                     .pSysMem = DATA(scene, 0, indices),
//                                 });
//    enemy.count = COUNT(scene, 0, indices);

// #pragma endregion

// #pragma region player
//    player.points = createShaderResourceView(createBuffer({
//                                                              .ByteWidth = LEN(scene, 1, attributes["POSITION"]),
//                                                              .BindFlags = D3D11_BIND_SHADER_RESOURCE,
//                                                              .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
//                                                              .StructureByteStride = sizeof(vec3),
//                                                          },
//                                                          {
//                                                              .pSysMem = DATA(scene, 1, attributes["POSITION"]),
//                                                          }));

//    player.normals = createShaderResourceView(createBuffer({
//                                                               .ByteWidth = LEN(scene, 1, attributes["NORMAL"]),
//                                                               .BindFlags = D3D11_BIND_SHADER_RESOURCE,
//                                                               .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
//                                                               .StructureByteStride = sizeof(vec3),
//                                                           },
//                                                           {
//                                                               .pSysMem = DATA(scene, 1, attributes["NORMAL"]),
//                                                           }));

//    player.uvs = createShaderResourceView(createBuffer({
//                                                           .ByteWidth = LEN(scene, 1, attributes["TEXCOORD_0"]),
//                                                           .BindFlags = D3D11_BIND_SHADER_RESOURCE,
//                                                           .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
//                                                           .StructureByteStride = sizeof(vec2),
//                                                       },
//                                                       {
//                                                           .pSysMem = DATA(scene, 1, attributes["TEXCOORD_0"]),
//                                                       }));

//    player.indices = createBuffer({
//                                      .ByteWidth = LEN(scene, 1, indices),
//                                      .BindFlags = D3D11_BIND_INDEX_BUFFER,
//                                  },
//                                  {
//                                      .pSysMem = DATA(scene, 1, indices),
//                                  });
//    player.count = COUNT(scene, 1, indices);

//    player.diffuse = createTexture({
//                                       .Width = u32(scene.images[1].width),
//                                       .Height = u32(scene.images[1].height),
//                                       .MipLevels = BIT_WIDTH(scene.images[1].width),
//                                       .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
//                                       .MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS,
//                                   },
//                                   {
//                                       .pSysMem = scene.images[1].image.data(),
//                                   });
// #pragma endregion

// #pragma region level
//    level.points = createShaderResourceView(createBuffer({
//                                                             .ByteWidth = LEN(scene, 2, attributes["POSITION"]),
//                                                             .BindFlags = D3D11_BIND_SHADER_RESOURCE,
//                                                             .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
//                                                             .StructureByteStride = sizeof(vec3),
//                                                         },
//                                                         {
//                                                             .pSysMem = DATA(scene, 2, attributes["POSITION"]),
//                                                         }));

//    level.normals = createShaderResourceView(createBuffer({
//                                                              .ByteWidth = LEN(scene, 2, attributes["NORMAL"]),
//                                                              .BindFlags = D3D11_BIND_SHADER_RESOURCE,
//                                                              .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
//                                                              .StructureByteStride = sizeof(vec3),
//                                                          },
//                                                          {
//                                                              .pSysMem = DATA(scene, 2, attributes["NORMAL"]),
//                                                          }));
//    level.indices = createBuffer({
//                                     .ByteWidth = LEN(scene, 2, indices),
//                                     .BindFlags = D3D11_BIND_INDEX_BUFFER,
//                                 },
//                                 {
//                                     .pSysMem = DATA(scene, 2, indices),
//                                 });

//    level.count = COUNT(scene, 2, indices);

//    level.diffuse = createTexture({
//                                      .Width = u32(scene.images[2].width),
//                                      .Height = u32(scene.images[2].height),
//                                      .MipLevels = BIT_WIDTH(scene.images[2].width),
//                                      .Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
//                                      .MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS,
//                                  },
//                                  {
//                                      .pSysMem = scene.images[2].image.data(),
//                                  });
// #pragma endregion

//    player.curPos = {float(scene.nodes[1].translation[0]), float(scene.nodes[1].translation[1]), float(scene.nodes[1].translation[2])};
//    player.prevPos = player.curPos;

//    auto vsTriangle = createVertexShader(L"out/vsTriangle.dxbc");
//    auto vsShadow = createVertexShader(L"out/vsShadow.dxbc");
//    auto vsMain = createVertexShader(L"out/vsMain.dxbc");
//    auto vsTriplanar = createVertexShader(L"out/vsTriplanar.dxbc");

//    auto psTriangle = createPixelShader(L"out/psTriangle.dxbc");
//    auto psMain = createPixelShader(L"out/psMain.dxbc");
//    auto psShadow = createPixelShader(L"out/psShadow.dxbc");

//    cb<mat4> model, view, proj, cascades[4]; // cascades transform world space to shadow
//    cb<vec3> light;

//    mat3 shadowToWorldRotation = euler(-pi / 6, pi / 3, 0);
//    light = -shadowToWorldRotation * vec3{0, 0, 1};

//    print("hello world");

//    LARGE_INTEGER prev, freq;
//    QueryPerformanceCounter(&prev);
//    QueryPerformanceFrequency(&freq);
//    ShowWindow(hwnd, SW_SHOW);
//    // ShowCursor(false);
//    while (true)
//    {
//       MSG msg;
//       while (PeekMessage(&msg, null, 0, 0, PM_REMOVE))
//       {
//          TranslateMessage(&msg);
//          DispatchMessage(&msg);
//       }

//       LARGE_INTEGER now;
//       QueryPerformanceCounter(&now);
//       float dt = float(now.QuadPart - prev.QuadPart) / freq.QuadPart;
//       prev = now;
//       physics.elapsed += dt;
//       while (physics.elapsed > 1 / physics.rate)
//       {
//          physics.elapsed -= 1 / physics.rate;
//          player.prevPos = player.curPos;

//          mat3 playerToWorldRotation = euler(player.yaw, 0, 0);

//          vec3 playerRight = playerToWorldRotation * vec3{1, 0, 0};
//          vec3 playerUp = playerToWorldRotation * vec3{0, 1, 0};
//          vec3 playerForward = playerToWorldRotation * vec3{0, 0, 1};

//          vec3 dir = {0, 0, 0};
//          if (keyDown['W']) dir += playerForward; // depends on camera orientation
//          if (keyDown['S']) dir -= playerForward;
//          if (keyDown['D']) dir += playerRight;
//          if (keyDown['A']) dir -= playerRight;
//          if (keyDown['E']) dir += playerUp;
//          if (keyDown['Q']) dir -= playerUp;
//          if (!eq(dir, {0, 0, 0}))
//             dir = normalize(dir);

//          player.curVel = dir * 8;

//          // start collision routine
//          // https://www.peroxide.dk/papers/collision/collision.pdf
//          // find the smallest t such that t*v is inside the triangle's "distance field"

//          vec3 distance = player.curVel / physics.rate;

//          int maxIterations = 6; // set this to max number of faces that can meet at a vertex (in theory, that's how many iterations are necessary, assuming the player doesn't move too quickly)
//          int count = 0;

//          while (count < maxIterations)
//          {
//             // test collisions against the inner ball. if we collide, then we resolve collision on the outer ball
//             // this is to solve the fundamental problem with point-volume testing: it's ill conditioned on the boundary
//             bool collided = false;
//             float t = 1;
//             vec3 normal;

//             range (i, COUNT(scene, 2, indices) / 3)
//             {
//                vec3 p = ((vec3*)DATA(scene, 2, attributes["POSITION"]))[((u16*)DATA(scene, 2, indices))[3 * i]] - player.curPos;
//                vec3 q = ((vec3*)DATA(scene, 2, attributes["POSITION"]))[((u16*)DATA(scene, 2, indices))[3 * i + 1]] - player.curPos;
//                vec3 r = ((vec3*)DATA(scene, 2, attributes["POSITION"]))[((u16*)DATA(scene, 2, indices))[3 * i + 2]] - player.curPos;

//                vec3 n = normalize(cross(q - p, r - p));                        // the plane is given by the equation dot(n, x) = dot(n, p)
//                float t1 = (dot(n, p) - player.innerRadius) / dot(n, distance); // solve dot(t*v, n) = dot(n, p) - innerRadius.
//                float t2 = (dot(n, p) + player.innerRadius) / dot(n, distance);

//                auto handleCollision = [&](float time, vec3 pointOnPlane)
//                {
//                   vec2 c = leftInverse(columns(q - p, r - p)) * (pointOnPlane - p);
//                   if (c[0] > 0 && c[0] < 1 && c[1] > 0 && c[1] < 1 && c[0] + c[1] < 1)
//                   {
//                      collided = true;
//                      t = time;
//                      normal = n;
//                   }
//                };

//                // wlog you can draw the normal upwards. mind the signs to compute the point on the original plane
//                if (t1 > 0 && t1 < t)
//                   handleCollision(t1, t1 * distance + n * player.innerRadius);
//                if (t2 > 0 && t2 < t)
//                   handleCollision(t2, t2 * distance - n * player.innerRadius);

//                auto handleCylinder = [&](vec3 p, vec3 q) { // infinite cylinder from p to q
//                   float len = length(q - p);
//                   vec3 a = (q - p) / len;

//                   // solve t*v on the infinite cylinder
//                   auto pCrossA{cross(p, a)};
//                   auto vCrossA{cross(distance, a)};
//                   auto numeratorPart{dot(vCrossA, pCrossA)};
//                   auto discriminant{dot(vCrossA, vCrossA) * square(player.innerRadius) - square(dot(p, vCrossA))};
//                   auto denom{dot(vCrossA, vCrossA)};

//                   auto t1{(numeratorPart + sqrt(discriminant)) / denom};
//                   auto t2{(numeratorPart - sqrt(discriminant)) / denom};

//                   auto handleCollision = [&](float time)
//                   {
//                      auto pos{distance * time};
//                      auto signedDistance{dot(a, pos - p)};
//                      if (signedDistance > 0 && signedDistance < len)
//                      {
//                         collided = true;
//                         t = time;
//                         normal = normalize(pos - (p + a * signedDistance));
//                      }
//                   };

//                   // any NAN/INF would be ignored
//                   if (t1 > 0 && t1 < t)
//                      handleCollision(t1);
//                   if (t2 > 0 && t2 < t)
//                      handleCollision(t2);
//                };
//                handleCylinder(p, q);
//                handleCylinder(q, r);
//                handleCylinder(r, p);

//                auto handleSphere = [&](vec3 p)
//                {
//                   auto vDotV{dot(distance, distance)};
//                   auto pDotP{dot(p, p)};
//                   auto vDotP{dot(distance, p)};

//                   auto numeratorPart{vDotP};
//                   auto discriminant{square(vDotP) - vDotV * (pDotP - square(player.innerRadius))};
//                   auto denom{vDotV};

//                   auto t1{(numeratorPart + sqrt(discriminant)) / denom};
//                   auto t2{(numeratorPart - sqrt(discriminant)) / denom};

//                   auto handleCollision = [&](float time)
//                   {
//                      auto pos{distance * time};
//                      collided = true;
//                      t = time;
//                      normal = normalize(pos - p);
//                   };
//                   if (t1 > 0 && t1 < t)
//                      handleCollision(t1);
//                   if (t2 > 0 && t2 < t)
//                      handleCollision(t2);
//                };

//                handleSphere(p);
//                handleSphere(q);
//                handleSphere(r);
//             }
//             if (collided)
//             {
//                // if collided, push the outer radius out of the collision to resolve the previous penetration
//                player.curPos += t * distance;
//                // note: we use the approximation sin(x) = x. technically you're supposed to move so that
//                // the outer radius ball is tangent to the plane.
//                player.curPos -= normalize(distance) * (player.outerRadius - player.innerRadius);

//                distance = (1 - t) * distance + (player.outerRadius - player.innerRadius) * normalize(distance); // also add the removed component to distance
//                distance -= dot(distance, normal) * normal;                                                      // find component orthogonal to the normal
//             }
//             else
//             {
//                player.curPos += distance;
//                break;
//             }
//             count++;
//          }
//       }

//       vec3 playerPos = lerp(player.prevPos, player.curPos, physics.elapsed * physics.rate);
//       player.yaw = cam.yaw;

//       mat3 viewToWorldRotation = euler(cam.yaw, cam.pitch, 0);

//       vec3 camRight = viewToWorldRotation * vec3{1, 0, 0};
//       vec3 camUp = viewToWorldRotation * vec3{0, 1, 0};
//       vec3 camForward = viewToWorldRotation * vec3{0, 0, 1};

//       cam.pos = playerPos - camForward * cam.armLength;

//       // if (keyDown['W']) cam.pos += camForward * cam.speed * dt;
//       // if (keyDown['S']) cam.pos -= camForward * cam.speed * dt;
//       // if (keyDown['A']) cam.pos -= camRight * cam.speed * dt;
//       // if (keyDown['D']) cam.pos += camRight * cam.speed * dt;

//       float aspect = windowSize[0] / windowSize[1];
//       view = affine(transpose(viewToWorldRotation), vec3{}) * affine(id<3>(), -cam.pos);
//       proj = {
//           1 / tan(cam.fov / 2), 0, 0, 0,
//           0, aspect / tan(cam.fov / 2), 0, 0,
//           0, 0, 0, cam.near,
//           0, 0, 1, 0};

// #pragma region update cascades

//       float d[] = {cam.near, 4, 10, 20, 50};
//       range (i, 4)
//       {
//          float x1 = d[i] * tan(cam.fov / 2);
//          float y1 = x1 / aspect;
//          float x2 = d[i + 1] * tan(cam.fov / 2);
//          float y2 = x2 / aspect;
//          vec3 frustumPoints[] = {
//              {x1, -y1, d[i]},
//              {x1, y1, d[i]},
//              {-x1, y1, d[i]},
//              {-x1, -y1, d[i]},
//              {x2, -y2, d[i + 1]},
//              {x2, y2, d[i + 1]},
//              {-x2, y2, d[i + 1]},
//              {-x2, -y2, d[i + 1]},
//          };
//          float xMin, yMin, zMin, xMax, yMax, zMax;
//          xMin = yMin = zMin = FLT_MAX;
//          xMax = yMax = zMax = -FLT_MAX;
//          range (i, 8)
//          {
//             vec3 v = transpose(shadowToWorldRotation) * (viewToWorldRotation * frustumPoints[i] + cam.pos);
//             xMin = min(xMin, v[0]);
//             yMin = min(yMin, v[1]);
//             zMin = min(zMin, v[2]);
//             xMax = max(xMax, v[0]);
//             yMax = max(yMax, v[1]);
//             zMax = max(zMax, v[2]);
//          }
//          // minimize floating point error by making s an integer
//          float s = ceil(max(length(frustumPoints[0] - frustumPoints[6]), length(frustumPoints[4] - frustumPoints[6])));
//          float T = s / 2048;

//          // divide by s/2 to keep x, y in [-1, 1]
//          // you can imagine an s x s square in world space where rasterization takes place.
//          // normally you subtract a = (xMin + xMax) / 2.
//          // instead you can keep region aligned to integer multiples of s / 2048 x s / 2048 so that it's rasterized the same way
//          // by first aligning a to a bottom-left point and then subtracting that.
//          // in effect, you are shifting the (xMax - xMin) x (yMax - yMin) rectangle to the up and right by a value in [0, s / 2048)
//          cascades[i] = affine(scale(2 / s, 2 / s, 1 / (zMax - zMin)), {0, 0, 0}) *
//                        affine(transpose(shadowToWorldRotation), {-floor((xMin + xMax) / 2 / T) * T, -floor((yMin + yMax) / 2 / T) * T, -zMin});
//          //    affine(transpose(shadowToWorldRotation), {-(xMin + xMax) / 2, -(yMin + yMax) / 2, -zMin});
//       }

// #pragma endregion

//       setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//       setBlendState(null);

//       setDepthState(dsLess);
//       setRasterState(rsShadow);

//       setVertexShader(vsShadow);
//       setPixelShader(null);
//       setViewports({viewport(2048, 2048)});

//       range (i, 4)
//       {
//          setVertexConstants(0, {model, cascades[i]});
//          clearDepthStencilView(shadowMap[i], 1);
//          setRenderTargets({}, shadowMap[i]);
//          model = id<4>();
//          level.bind();
//          level.draw();
//          model = affine(euler(player.yaw, 0, 0), playerPos);
//          player.bind();
//          player.draw();
//       }

//       setVertexConstants(0, {model, view, proj});
//       setPixelConstants(0, {light, cascades[0], cascades[1], cascades[2], cascades[3]});

//       clearRenderTargetView(texWindow, rgba(139, 162, 191, 1));
//       clearDepthStencilView(depthWindow, 0); // reverse depth means 0 = far plane
//                                              // so error during world space -> view space -> clip space -> screen space
//                                              // conversion is spread out more evenly (projection matrix defines a
//                                              // function z |-> near/z)
//       setRenderTargets({texWindow}, depthWindow);
//       setViewports({viewport(windowSize[0], windowSize[1])});
//       setDepthState(dsGreater);
//       setRasterState(rsFill);

//       setVertexShader(vsTriplanar);
//       setPixelShader(psShadow);
//       setPixelResources(1, {shadowMap[0], shadowMap[1], shadowMap[2], shadowMap[3]});
//       setSamplers(0, {ssGrid, ssCmp});

//       model = id<4>();
//       level.bind();
//       level.draw();

//       setVertexShader(vsMain);
//       setPixelShader(psMain);
//       model = affine(euler(player.yaw, 0, 0), playerPos);
//       player.bind();
//       player.draw();

//       setPixelResources(1, {null, null, null, null});

//       spriteBatch.Begin();
//       spriteFont.DrawString(&spriteBatch, player.state == Player::Ground ? L"Ground" : L"Air", Vector2(100, 100));
//       auto width = spriteFont.MeasureString(L"Hello, world!");
//       spriteBatch.End();

//       sc->Present(1, 0);
//    }

//    return 0;
// }

// #include <iostream>
// #include <set>
// #include <vector>

// using namespace std;
// int main()
// {
//    int N, M;
//    cin >> N >> M;
//    auto adj = vector<vector<pair<int, int>>>(N + 1);
//    auto dis = vector<int>(N + 1);

//    for (int i = 0; i < M; i++)
//    {
//       int u, v, w;
//       cin >> u >> v >> w;
//       adj[u].push_back({v, w}); // vector<T>& passed by reference and therefore complexity here is O(1)
//       adj[v].push_back({u, w});
//    }

//    set<pair<int, int>> s;
//    s.insert({0, 1});

//    for (int i = 2; i <= N; i++)
//    {
//       dis[i] = 1e9;
//       s.insert({dis[i], i});
//    }

//    while (!s.empty())
//    {
//       auto [d, n] = *s.begin();
//       s.erase({d, n});
//       for (auto [next, w] : adj[n])
//       {
//          if (d + w < dis[next])
//          {
//             s.erase({dis[next], next});
//             dis[next] = d + w;
//             s.insert({dis[next], next});
//          }
//       }
//    }

//    for (int i = 1; i <= N; i++) cout << (dis[i] == 1e9 ? -1 : dis[i]) << endl;
// }

// lower_bound/upper_bound example
// #include <algorithm> // std::lower_bound, std::upper_bound, std::sort
// #include <iostream>  // std::cout
// #include <vector>    // std::vector

// int main()
// {
//    int myints[] = {10, 20, 30, 30, 20, 10, 10, 20};
//    std::vector<int> v(myints, myints + 8); // 10 20 30 30 20 10 10 20

//    std::sort(v.begin(), v.end()); // 10 10 10 20 20 20 30 30

//    std::vector<int>::iterator low, up;
//    low = std::lower_bound(v.begin(), v.end(), 20); //          ^
//    up = std::upper_bound(v.begin(), v.end(), 30);  //                   ^

//    std::cout << "lower_bound at position " << (low - v.begin()) << '\n';
//    std::cout << "upper_bound at position " << (up - v.begin()) << '\n';

//    int sz = sizeof(std::vector<int>::iterator);
//    // &vector<int>{1, 2, 3};

//    int lo = 0, hi = v.size() - 1;
//    int pos = v.size();
//    while (lo <= hi)
//    {
//       int mid = (lo + hi) / 2;
//       if (v[mid] > 30)
//       {
//          pos = mid;
//          hi = mid - 1;
//       }
//       else
//          lo = mid + 1;
//    }
//    std::cout << pos << std::endl;

//    return 0;
// }

// #include <iostream>
// #include <string>
// #include <vector>
// using namespace std;

// bool isSubSeqRec(string& s1, string& s2, int m, int n)
// {
//    // Base Cases
//    if (m == 0)
//       return true;
//    if (n == 0)
//       return false;

//    // If last characters match
//    if (s1[m - 1] == s2[n - 1])
//       return isSubSeqRec(s1, s2, m - 1, n - 1);

//    // If last characters don't match
//    return isSubSeqRec(s1, s2, m, n - 1);
// }

// bool isSubSeq(string& s1, string& s2)
// {
//    int m = s1.length();
//    int n = s2.length();
//    if (m > n) return false;
//    return isSubSeqRec(s1, s2, m, n);
// }

// int main()
// {
//    string s1 = "gksrek";
//    string s2 = "geeksforgeeks";
//    for (int i = 0; i < 10000; i++) s2 += 'a';

//    // vector<int> v;
//    if (isSubSeq(s1, s2))
//       cout << "true";
//    else
//       cout << "false";
//    return 0;
// }

#include <chrono>
#include <iostream>
#include <set>
#include <vector>

using namespace std;

// pass by reference: changes are reflected in the original object and O(1) time is spent copying
void f(vector<int> v, int x)
{
}

int main()
{
#include <chrono>

   vector<int> v(100000);
   auto start = std::chrono::high_resolution_clock::now();
   for (int i = 0; i < (int)v.size(); ++i)
   {
      f(v, i);
   }
   auto end = std::chrono::high_resolution_clock::now();
   auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
   cout << "elapsed: " << elapsed_us << " us\n";
}