#include "inc.h"
#include "shared.h"
#include "auto.h"

#define POSITION "POSITION"
#define NORMAL "NORMAL"
#define TEXCOORD_0 "TEXCOORD_0"
#define JOINTS_0 "JOINTS_0"
#define WEIGHTS_0 "WEIGHTS_0"
#define COLOR "COLOR"

struct Timer {

    LARGE_INTEGER prev;
    LARGE_INTEGER freq;

    Timer()
    {
        QueryPerformanceCounter(&prev);
        QueryPerformanceFrequency(&freq);
    }

    float update()
    {
        _ now {LARGE_INTEGER()};
        QueryPerformanceCounter(&now);
        _ dt {float(now.QuadPart - prev.QuadPart) / freq.QuadPart};
        prev = now;
        return dt;
    }
};

struct Mesh {
    vec<vec3> pos;
    vec<ushort3> index;
    bool walkable;
};
struct Buffer: map<str, ComPtr<ID3D11ShaderResourceView>> {
    ComPtr<ID3D11Buffer> index;
    int indexCount;
};
struct Level {
    vec<Buffer> buffers;
    vec<Mesh> meshes;
};
struct EulerTransform {
    vec3 pos;
    float yaw;
    float pitch;
    float roll;

    mat3 rotation() { return mat3::rotate(yaw, pitch, roll); }
    mat3 invRotation() { return transpose(rotation()); }

    vec3 right() { return rotation() * vec3::right(); }
    vec3 up() { return rotation() * vec3::up(); }
    vec3 forward() { return rotation() * vec3::forward(); }

    mat4 worldToLocal() { return mat4::affine(invRotation()) * mat4::translate(-pos); }
    mat4 localToWorld() { return mat4::affine(rotation(), pos); }
};

struct Camera: EulerTransform {
    float speed;
    float fov;
    float near;
    float sens;
    float armLength; 
};
struct Player: EulerTransform {
    enum State {
        Air,
        Ground,
    };
    vec3 curPos;
    vec3 prevPos;
    vec3 curVel;
    vec3 prevVel;
    vec3 accel;
    float maxGroundSpeed;
    State state;
    float outerRadius;
    float innerRadius;
    bool inShadow;
};
struct Physics {
    float rate;
    float elapsed;
};

_ gCam {Camera {
    // .speed {5.0f},
    .fov {deg2rad(103)},
    .near {0.1f},
    .sens {0.002f},
    .armLength {5.0f},
}};
_ gCursorLocked {false};
_ gKeyDown {map<u16, bool>()};
_ gWindowStyle {WS_OVERLAPPEDWINDOW ^ (WS_SIZEBOX | WS_MAXIMIZEBOX)};
_ [gFullscreen, gMsaaCount, gBufferCount, gSwapChainFlags] {[&] {
    _ f {ifstream("settings.json")};
    _ js {json::parse(f)};
    return tuple(js["fullscreen"], js["msaaCount"], js["bufferCount"], DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
}()};

// note: you CANNOT modify the buffer when xaudio2 is playing it
// https://stackoverflow.com/questions/46831684/xaudio2-cracking-output-when-using-a-dynamic-buffer
_ gWindow {HWND()};
_ gWidth {0.0f};
_ gHeight {0.0f};
_ gRtvWindow {ComPtr<ID3D11RenderTargetView>()};
_ gDsvWindow {ComPtr<ID3D11DepthStencilView>()};
_ gTexWindow {ComPtr<ID3D11Texture2D>()};
_ gDepthWindow {ComPtr<ID3D11Texture2D>()};
_ gDevice {ComPtr<ID3D11Device>()};
_ gContext {ComPtr<ID3D11DeviceContext>()};
_ gSwapChain {ComPtr<IDXGISwapChain2>()};
_ gWaitableObject {HANDLE()};
_ gSampleRate {44100};
_ [gSquareWave] {[&] {
    _ freq {440};
    _ duration {2};

    _ numSamples {gSampleRate * duration};
    _ squareWave {vec<float>(numSamples)};

    for (_ i : range(numSamples))
    {
        squareWave[i] = 1 * sin(tau * i * freq / gSampleRate) +
                        (1.0f / 3) * sin(tau * i * 3 * freq / gSampleRate) +
                        (1.0f / 5) * sin(tau * i * 5 * freq / gSampleRate) +
                        (1.0f / 7) * sin(tau * i * 7 * freq / gSampleRate) +
                        (1.0f / 9) * sin(tau * i * 9 * freq / gSampleRate);
    }

    _ M {rmaxabs(squareWave)};
    squareWave = fmap(squareWave, [&](float x) { return x / M; });

    return tuple(squareWave);
}()};

template<type T>
struct ConstantBuffer: T, ComPtr<ID3D11Buffer> {

    void init(ComPtr<ID3D11Device> dev)
    {
        dev->CreateBuffer(&D3D11_BUFFER_DESC {
                              .ByteWidth {(sizeof(T) + 15) & ~15},
                              .Usage {D3D11_USAGE_DYNAMIC},
                              .BindFlags {D3D11_BIND_CONSTANT_BUFFER},
                              .CPUAccessFlags {D3D11_CPU_ACCESS_WRITE},
                          },
                          __, &this); // use comptr operator&
    }

    // allow partial updates where the rest is discarded.
    // useful for bone matrices where different objects have a different number of bones
    void update(ComPtr<ID3D11DeviceContext> ctx, int size = sizeof(T))
    {
        assert(size <= sizeof(T));

        _ mapped {D3D11_MAPPED_SUBRESOURCE()};
        ctx->Map(this, __, D3D11_MAP_WRITE_DISCARD, __, &mapped); // this is converted to ID3D11Resource* (using comptr<>'s functionality)
        memcpy(mapped.pData, addressof(this), size);              // since we inherited from ptr<>, operator& was overloaded. rely on the memory layout
        ctx->Unmap(this, __);
    }
};

_ cbModel {ConstantBuffer<CBModel>()};
_ cbPerFrame {ConstantBuffer<CBPerFrame>()};
_ cbPerResize {ConstantBuffer<CBPerResize>()};
_ cbShadow {ConstantBuffer<CBShadow>()};
_ cbGlyph {ConstantBuffer<CBGlyph>()};

_[gAudio, gMasteringVoice, gSquareVoice] {[&] {
    check(CoInitialize(__));
    _ audio {ComPtr<IXAudio2>()};
    _ masteringVoice {ptr<IXAudio2MasteringVoice>()};
    _ squareVoice {ptr<IXAudio2SourceVoice>()};
    check(XAudio2Create(&audio));
#ifdef DEBUG
    audio->SetDebugConfiguration(&XAUDIO2_DEBUG_CONFIGURATION {
        .TraceMask {XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS},
        .BreakMask {XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS},
    });
#endif

    check(audio->CreateMasteringVoice(&masteringVoice));
    check(audio->CreateSourceVoice(&squareVoice, &WAVEFORMATEX {
                                                     .wFormatTag {WAVE_FORMAT_IEEE_FLOAT},
                                                     .nChannels {1},
                                                     .nSamplesPerSec {u32(gSampleRate)},
                                                     .nAvgBytesPerSec {u32(gSampleRate * sizeof(float))},
                                                     .nBlockAlign {sizeof(float)},
                                                     .wBitsPerSample {sizeof(float) * 8},
                                                 }));

    check(squareVoice->SubmitSourceBuffer(&XAUDIO2_BUFFER {
        .AudioBytes {u32(gSquareWave.size() * sizeof(float))},
        .pAudioData {(u8 *)gSquareWave.data()},
        .LoopCount {XAUDIO2_LOOP_INFINITE},
    }));
    return tuple(audio, masteringVoice, squareVoice);
}()};

void onKeyPressed(u16 key)
{
    switch (key)
    {
        case VK_ESCAPE:
            ExitProcess(0);
            break;
        case 'Z':
            gCursorLocked = !gCursorLocked;
            if (gCursorLocked)
            {
                _ rect {RECT()};
                GetClientRect(gWindow, &rect);
                _ center {POINT(rect.right / 2, rect.bottom / 2)};
                ClientToScreen(gWindow, &center);
                ClipCursor(&RECT(center.x, center.y, center.x, center.y));

                ShowCursor(false);
            }
            else
            {
                ClipCursor(__);
                ShowCursor(true);
            }
            break;
        case VK_UP:
            gSquareVoice->Start();
            break;
        case VK_DOWN:
            gSquareVoice->Stop();
            break;

        case VK_F11:
            gFullscreen = !gFullscreen;
            if (gFullscreen)
            {
                SetWindowLong(gWindow, GWL_STYLE, 0);
                ShowWindow(gWindow, SW_SHOWMAXIMIZED);
            }
            else
            {
                _ rect {RECT(0, 0, 1280, 720)};
                AdjustWindowRect(&rect, gWindowStyle, __); // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrect
                SetWindowLong(gWindow, GWL_STYLE, gWindowStyle);
                SetWindowPos(gWindow, __, __, __, rect.right - rect.left, rect.bottom - rect.top, __);
                ShowWindow(gWindow, SW_SHOW);
            }
            break;
    }
}
void onKeyReleased(u16 key)
{
}
void onMouseMove(int dx, int dy)
{
    if (gCursorLocked)
    {
        gCam.pitch = clamp(gCam.pitch - dy * gCam.sens, -tau / 4, tau / 4);
        gCam.yaw = fmod(gCam.yaw - dx * gCam.sens, tau);
    }
}
LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_MOUSEWHEEL:
            // gCam.speed *= pow(1.5f, float(GET_WHEEL_DELTA_WPARAM(wp)) / 120);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);

            break;

        case WM_SIZE:
        {
            _ width {LOWORD(lp)};
            _ height {HIWORD(lp)};
            if (width > 0 && height > 0)
            {
                print("WM_SIZE", width, height);
                gWidth = width;
                gHeight = height;
                gTexWindow.Reset();
                gRtvWindow.Reset();
                gSwapChain->ResizeBuffers(__, __, __, __, gSwapChainFlags);
                gSwapChain->GetBuffer(0, IID_PPV_ARGS(&gTexWindow));

                gDevice->CreateRenderTargetView(gTexWindow, &D3D11_RENDER_TARGET_VIEW_DESC {
                                                                .Format {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
                                                                .ViewDimension {D3D11_RTV_DIMENSION_TEXTURE2D},
                                                            },
                                                &gRtvWindow);

                gDevice->CreateTexture2D(&D3D11_TEXTURE2D_DESC {
                                             .Width {u32(width)},
                                             .Height {u32(height)},
                                             .MipLevels {1},
                                             .ArraySize {1},
                                             .Format {DXGI_FORMAT_D32_FLOAT},
                                             .SampleDesc {.Count {1}},
                                             .BindFlags {D3D11_BIND_DEPTH_STENCIL},
                                         },
                                         __, &gDepthWindow);

                gDevice->CreateDepthStencilView(gDepthWindow, __, &gDsvWindow);

                cbPerResize.winWidth = gWidth;
                cbPerResize.winHeight = gHeight;
                cbPerResize.update(gContext);
            }
        }
        break;

        case WM_INPUT:
        {
            _ raw {RAWINPUT()};
            _ size {UINT(sizeof(RAWINPUT))};
            GetRawInputData(HRAWINPUT(lp), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
            switch (raw.header.dwType)
            {
                case RIM_TYPEKEYBOARD:
                {
                    _ keyboard {raw.data.keyboard};
                    _ key {keyboard.VKey};
                    if (keyboard.Message == WM_KEYDOWN && !gKeyDown[key])
                    {
                        gKeyDown[key] = true;
                        onKeyPressed(key);
                    }
                    else if (keyboard.Message == WM_KEYUP && gKeyDown[key])
                    {
                        gKeyDown[key] = false;
                        onKeyReleased(key);
                    }
                    break;
                }
                case RIM_TYPEMOUSE:
                {
                    _ mouse {raw.data.mouse};
                    if (mouse.usFlags == MOUSE_MOVE_RELATIVE) // for mouse movement, not tablet
                        onMouseMove(mouse.lLastX, mouse.lLastY);
                }
            }
        }
        break;

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

// https://ruby0x1.github.io/machinery_blog_archive/post/writing-a-low-level-sound-system/index.html
// https://learn.microsoft.com/en-us/windows/win32/xaudio2/full-project
int WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

    {
        RegisterClassEx(&WNDCLASSEX {
            .cbSize {sizeof(WNDCLASSEX)},
            .lpfnWndProc {wndProc}, // in c++ functions are actually function variables, which can implicitly convert to function types
            .hIcon {LoadImage(__, "assets/game.ico", IMAGE_ICON,
                              GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE)},
            .hCursor {LoadCursor(__, IDC_ARROW)},
            .lpszClassName {"Editor"},
            .hIconSm {LoadImage(__, "assets/game.ico", IMAGE_ICON,
                                GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE)},
        });

        _ rect {RECT(0, 0, 1280, 720)};
        AdjustWindowRect(&rect, gWindowStyle, false); // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrect

        gWindow = CreateWindow("Editor", "Editor", gWindowStyle,
                               0, 0, rect.right - rect.left, rect.bottom - rect.top, __, __, __, __);

        RegisterRawInputDevices(vec<RAWINPUTDEVICE> {
                                    {
                                        .usUsagePage {HID_USAGE_PAGE_GENERIC},
                                        .usUsage {HID_USAGE_GENERIC_MOUSE},
                                    },
                                    {
                                        .usUsagePage {HID_USAGE_PAGE_GENERIC},
                                        .usUsage {HID_USAGE_GENERIC_KEYBOARD},
                                        .dwFlags {RIDEV_NOLEGACY},
                                    },
                                }
                                    .data(),
                                2, sizeof(RAWINPUTDEVICE));
    }

    // note: texWindow is for multisample resolve
    {
        _ factory {ComPtr<IDXGIFactory6>()};
        check(CreateDXGIFactory2(
#ifdef DEBUG
            DXGI_CREATE_FACTORY_DEBUG,
#else
            0,
#endif
            IID_PPV_ARGS(&factory)));

        _ adapter {ComPtr<IDXGIAdapter1>()};
        check(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));

        check(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, __,
#ifdef DEBUG
                                D3D11_CREATE_DEVICE_DEBUG,
#else
                                0,
#endif
                                __, __, D3D11_SDK_VERSION, &gDevice, __, &gContext));

#ifdef DEBUG
        _ info {ComPtr<ID3D11InfoQueue>()};
        gDevice.As(&info);
        info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
        info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
        // info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);
#endif

        _ sc {ComPtr<IDXGISwapChain1>()};
        check(factory->CreateSwapChainForHwnd(gDevice, gWindow,
                                              &DXGI_SWAP_CHAIN_DESC1 {
                                                  .Format {DXGI_FORMAT_R8G8B8A8_UNORM},
                                                  .SampleDesc {.Count {1}},
                                                  .BufferUsage {DXGI_USAGE_RENDER_TARGET_OUTPUT},
                                                  .BufferCount {gBufferCount},
                                                  .SwapEffect {DXGI_SWAP_EFFECT_FLIP_DISCARD},
                                                  .Flags {u32(gSwapChainFlags)},
                                              },
                                              __, __, &sc));
        check(sc.As(&gSwapChain));
        gWaitableObject = gSwapChain->GetFrameLatencyWaitableObject();
    }

    _ [vs, ps] {[&] {
        _ vsEntries {map<str, pair<str, vec<str>>> {
            {"clip", {"vsMain", __}},
            {"pos", {"vsMain", {POSITION}}},
            {"posNormalUV", {"vsMain", {POSITION, NORMAL, TEXCOORD_0}}},
            {"normal", {"vsNormal", {COLOR}}},
            {"shadow", {"vsShadow", __}},
            {"font", {"vsFont", {TEXCOORD_0}}},
            {"axis", {"vsAxis", {COLOR}}},
        }};
        _ psEntries {map<str, pair<str, vec<str>>> {
            {"color", {"psColor", {COLOR}}},
            {"skybox", {"psSkybox", {POSITION}}},
            {"lambert", {"psLambert", {POSITION, NORMAL, TEXCOORD_0}}},
            {"lambertInShadow", {"psLambert", {POSITION, NORMAL, TEXCOORD_0, "IN_SHADOW"}}},
            {"lambertReceiveShadows", {"psLambert", {POSITION, NORMAL, TEXCOORD_0, "RECEIVE_SHADOWS"}}},
            {"lambertReceiveShadowsShowCascades", {"psLambert", {POSITION, NORMAL, TEXCOORD_0, "RECEIVE_SHADOWS", "SHOW_CASCADES"}}},
            {"solid", {"psSolid", __}},
            {"font", {"psFont", {TEXCOORD_0}}},
        }};
        _ vs {map<str, ComPtr<ID3D11VertexShader>>()};
        _ ps {map<str, ComPtr<ID3D11PixelShader>>()};

        _ compile {[](str entry, vec<str> defines, str target) {
            _ def {vec<D3D_SHADER_MACRO>()};
            for (_ i : range(defines.size()))
                def.push({defines[i].data(), __});
            def.push(__);

            _ blob {ComPtr<ID3DBlob>()};
            _ errOrWarn {ComPtr<ID3DBlob>()};

            _ flags {D3DCOMPILE_PACK_MATRIX_ROW_MAJOR};
#ifdef DEBUG
            flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

            D3DCompileFromFile(L"src/shaders.hlsl", def.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE,
                               entry.data(), target.data(), flags, __, &blob, &errOrWarn);

            if (errOrWarn)
                print((char *)errOrWarn->GetBufferPointer());

            assert(blob);

            return blob;
        }};

        _ createVS {[&](str name, str entry, vec<str> defines) {
            _ blob(compile(entry, defines, "vs_5_0"));
            _ vs {ComPtr<ID3D11VertexShader>()};
            check(gDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), __, &vs));

#ifdef DEBUG
            _ dis {ComPtr<ID3DBlob>()};
            D3DDisassemble(blob->GetBufferPointer(), blob->GetBufferSize(), __, __, &dis);
            ofstream(format("vs/{}.asm", name)) << (char *)dis->GetBufferPointer();
#endif
            return vs;
        }};

        _ createPS {[&](str name, str entry, vec<str> defines) {
            _ blob(compile(entry, defines, "ps_5_0"));
            _ ps {ComPtr<ID3D11PixelShader>()};
            check(gDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), __, &ps));

#ifdef DEBUG
            _ dis {ComPtr<ID3DBlob>()};
            D3DDisassemble(blob->GetBufferPointer(), blob->GetBufferSize(), __, __, &dis);
            ofstream(format("ps/{}.asm", name)) << (char *)dis->GetBufferPointer();
#endif
            return ps;
        }};

        for (_ [name, shader] : vsEntries) vs[name] = createVS(name, shader.first, shader.second);
        for (_ [name, shader] : psEntries) ps[name] = createPS(name, shader.first, shader.second);

        return tuple(vs, ps);
    }()};
    _ [sampler, depthState, rasterState, blendState] {[&] {
        _ ssDesc {map<str, D3D11_SAMPLER_DESC> {
            {"grid", {
                         .Filter {D3D11_FILTER_ANISOTROPIC},
                         .AddressU {D3D11_TEXTURE_ADDRESS_WRAP},
                         .AddressV {D3D11_TEXTURE_ADDRESS_WRAP},
                         .AddressW {D3D11_TEXTURE_ADDRESS_WRAP},
                         .MaxAnisotropy {16},
                         .MinLOD {-FLT_MAX},
                         .MaxLOD {FLT_MAX},
                     }},
            // remember: addressUVW only influences the sample operation}, not the load operation
            {"cmpGreater", {
                               .Filter {D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT}, // magnification is when pixel to texel ratio is > 1
                               .AddressU {D3D11_TEXTURE_ADDRESS_CLAMP},
                               .AddressV {D3D11_TEXTURE_ADDRESS_CLAMP},
                               .AddressW {D3D11_TEXTURE_ADDRESS_CLAMP}, // seems to be the safest option.
                               .ComparisonFunc {D3D11_COMPARISON_GREATER},
                           }},
            // glyphs will have a constant pixel to texel ratio (so no mipmapping is required).
            // just make sure you leave a small gap between glyphs.
            {"font", {
                         .Filter {D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT},
                         .AddressU {D3D11_TEXTURE_ADDRESS_CLAMP},
                         .AddressV {D3D11_TEXTURE_ADDRESS_CLAMP},
                         .AddressW {D3D11_TEXTURE_ADDRESS_CLAMP},
                     }},
        }};
        _ dsDesc {map<str, D3D11_DEPTH_STENCIL_DESC> {
            {"greater", {
                            .DepthEnable {true},
                            .DepthWriteMask {D3D11_DEPTH_WRITE_MASK_ALL},
                            .DepthFunc {D3D11_COMPARISON_GREATER},
                        }},
            {"disabled", {.DepthEnable {false}}},
        }};
        _ rsDesc {map<str, D3D11_RASTERIZER_DESC> {
            {"shadow",
             {
                 // todo: https://www.youtube.com/watch?v=NCptEJ1Uevg
                 .FillMode {D3D11_FILL_SOLID},
                 .CullMode {D3D11_CULL_BACK},
                 // check fged 2nd edition for a detailed explanation of these values
                 .DepthBiasClamp {-0.05f},
                 .SlopeScaledDepthBias {-3.0f}, // when rasterizing a shadow map, bias away from the light (using DEPTH_GREATER)
                 .DepthClipEnable {false},      // pancake depth
             }},
            {"noCull",
             {
                 .FillMode {D3D11_FILL_SOLID},
                 .CullMode {D3D11_CULL_NONE},
                 .DepthClipEnable {false},
             }}, // for skybox, we don't need to clip to 0 <= z <= w
            {"wireframe",
             {
                 .FillMode {D3D11_FILL_WIREFRAME},
                 .CullMode {D3D11_CULL_BACK},
                 .DepthClipEnable {true},
             }},
        }};
        _ bsDesc {map<str, D3D11_BLEND_DESC> {

            {"alpha",
             {
                 .RenderTarget {
                     {
                         .BlendEnable {true},
                         .SrcBlend {D3D11_BLEND_SRC_ALPHA}, // newColor = srcColor * srcBlend + destColor * destBlend
                         .DestBlend {D3D11_BLEND_INV_SRC_ALPHA},
                         .BlendOp {D3D11_BLEND_OP_ADD},
                         .SrcBlendAlpha {D3D11_BLEND_ONE}, // newAlpha = srcAlpha * srcBlend + destAlpha * destBlend
                         .DestBlendAlpha {D3D11_BLEND_ZERO},
                         .BlendOpAlpha {D3D11_BLEND_OP_ADD},
                         .RenderTargetWriteMask {D3D11_COLOR_WRITE_ENABLE_ALL},
                     },
                 },
             }},
        }};
        return tuple(fmap(ssDesc, [&](D3D11_SAMPLER_DESC desc) {
                         ComPtr<ID3D11SamplerState> ss;
                         check(gDevice->CreateSamplerState(&desc, &ss));
                         return ss;
                     }),
                     fmap(dsDesc, [&](D3D11_DEPTH_STENCIL_DESC desc) {
                         ComPtr<ID3D11DepthStencilState> ds;
                         check(gDevice->CreateDepthStencilState(&desc, &ds));
                         return ds;
                     }),
                     fmap(rsDesc, [&](D3D11_RASTERIZER_DESC desc) {
                         ComPtr<ID3D11RasterizerState> rs;
                         check(gDevice->CreateRasterizerState(&desc, &rs));
                         return rs;
                     }),
                     fmap(bsDesc, [&](D3D11_BLEND_DESC desc) {
                         ComPtr<ID3D11BlendState> bs;
                         check(gDevice->CreateBlendState(&desc, &bs));
                         return bs;
                     }));
    }()};

#pragma region helpers
    _ setRenderTarget {[&](vec<ID3D11RenderTargetView *> rtvs, ID3D11DepthStencilView *dsv) {
        gContext->OMSetRenderTargets(rtvs.size(), rtvs.data(), dsv);
    }};
    _ clearRenderTarget {[&](ID3D11RenderTargetView *rtv, float r, float g, float b, float a) {
        gContext->ClearRenderTargetView(rtv, vec4(r, g, b, a).data());
    }};
    _ clearDepth {[&](ID3D11DepthStencilView *dsv, float depth) {
        gContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, depth, __);
    }};
    _ setViewport {[&](float width, float height) {
        gContext->RSSetViewports(1, &D3D11_VIEWPORT {.Width {width}, .Height {height}, .MaxDepth {1}});
    }};
    _ setVS {[&](str name) { 
            assert(vs.contains(name));
            gContext->VSSetShader(vs[name], __, __); }};
    _ setPS {[&](str name) {
        assert(ps.contains(name));
        gContext->PSSetShader(ps[name], __, __);
    }};
    _ clearPS([&] { gContext->PSSetShader(__, __, __); });
    _ setVSResources {[&](int start, vec<ID3D11ShaderResourceView *> srvs) {
        gContext->VSSetShaderResources(start, srvs.size(), srvs.data());
    }};
    _ setPSResources {[&](int start, vec<ID3D11ShaderResourceView *> srvs) {
        gContext->PSSetShaderResources(start, srvs.size(), srvs.data());
    }};
    _ clearPSResources {[&](int start, int count) {
        setPSResources(start, vec<ID3D11ShaderResourceView *>(count));
    }};
    _ setPSSamplers {[&](int start, vec<ID3D11SamplerState *> samplers) {
        gContext->PSSetSamplers(start, samplers.size(), samplers.data());
    }};
    _ setIndexBuffer {[&](ID3D11Buffer *ib) { gContext->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, __); }};
    _ setTopology {[&](D3D11_PRIMITIVE_TOPOLOGY topology) { gContext->IASetPrimitiveTopology(topology); }};
    _ setDepthState {[&](str name) {
        assert(depthState.contains(name));
        gContext->OMSetDepthStencilState(depthState[name], __);
    }};
    _ setRasterState {[&](str name) {
        assert(rasterState.contains(name));
        gContext->RSSetState(rasterState[name]);
    }};
    // default raster state: cull back faces, fill solid, and clip depth
    _ clearRasterState {[&] { gContext->RSSetState(__); }};
    _ setBlendState {[&](str name) {
        assert(blendState.contains(name));
        gContext->OMSetBlendState(blendState[name], __, 0xffffffff);
    }};
    _ clearBlendState {[&] { gContext->OMSetBlendState(__, __, 0xffffffff); }};
    _ draw {[&](int count) { gContext->Draw(count, __); }};
    _ drawIndexed {[&](int count) { gContext->DrawIndexed(count, __, __); }};

    _ setVSConstants {[&](int start, vec<ID3D11Buffer *> cbs) {
        gContext->VSSetConstantBuffers(start, cbs.size(), cbs.data());
    }};
    _ setPSConstants {[&](int start, vec<ID3D11Buffer *> cbs) {
        gContext->PSSetConstantBuffers(start, cbs.size(), cbs.data());
    }};

#pragma endregion

    _ [shadow, srvShadows, dsvShadows] {[&] {
        _ transform {EulerTransform {.yaw = tau / 12, .pitch = -tau / 6}};
        _ srvs {vec<ComPtr<ID3D11ShaderResourceView>>(CASCADES)};
        _ dsvs {vec<ComPtr<ID3D11DepthStencilView>>(CASCADES)};

        for (_ i : range(CASCADES))
        {
            ComPtr<ID3D11Texture2D> tex;
            gDevice->CreateTexture2D(&D3D11_TEXTURE2D_DESC {
                                         .Width {SHADOW_MAP},
                                         .Height {SHADOW_MAP},
                                         .MipLevels {1},
                                         .ArraySize {1},
                                         .Format {DXGI_FORMAT_R32_TYPELESS},
                                         .SampleDesc {.Count {1}},
                                         .BindFlags {D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE},
                                     },
                                     __, &tex);

            gDevice->CreateShaderResourceView(tex, &D3D11_SHADER_RESOURCE_VIEW_DESC {
                                                       .Format {DXGI_FORMAT_R32_FLOAT},
                                                       .ViewDimension {D3D11_SRV_DIMENSION_TEXTURE2D},
                                                       .Texture2D {.MipLevels {1}},
                                                   },
                                              &srvs[i]);

            gDevice->CreateDepthStencilView(tex, &D3D11_DEPTH_STENCIL_VIEW_DESC {
                                                     .Format {DXGI_FORMAT_D32_FLOAT},
                                                     .ViewDimension {D3D11_DSV_DIMENSION_TEXTURE2D},
                                                 },
                                            &dsvs[i]);
        }
        return tuple(transform, srvs, dsvs);
    }()};
    {
        cbModel.init(gDevice);
        cbPerFrame.init(gDevice);
        cbPerResize.init(gDevice);
        cbShadow.init(gDevice);
        cbGlyph.init(gDevice);

        setVSConstants(0, {cbModel, cbPerFrame, cbPerResize, cbShadow, cbGlyph});
        setPSConstants(0, {cbModel, cbPerFrame, cbPerResize, cbShadow, cbGlyph});
        cbPerResize.lightDir = -shadow.forward();
    }
    _ [srvDither, srvGrid, srvDin] {[&] {
        // https://github.com/microsoft/DirectXTK/wiki/WICTextureLoader
        _ dither {ComPtr<ID3D11ShaderResourceView>()};
        _ grid {ComPtr<ID3D11ShaderResourceView>()};
        _ din {ComPtr<ID3D11ShaderResourceView>()};
        _ tex {ComPtr<ID3D11Texture2D>()};

        gDevice->CreateTexture2D(&D3D11_TEXTURE2D_DESC {
                                     .Width {DITHER},
                                     .Height {DITHER},
                                     .MipLevels {1},
                                     .ArraySize {1},
                                     .Format {DXGI_FORMAT_R8_UNORM},
                                     .SampleDesc {.Count {1}},
                                     .BindFlags {D3D11_BIND_SHADER_RESOURCE},
                                 },
                                 &D3D11_SUBRESOURCE_DATA {
                                     .pSysMem {vec<u8>::load("assets/dither.bin").data()},
                                     .SysMemPitch {DITHER},
                                 },
                                 &tex);
        gDevice->CreateShaderResourceView(tex, __, &dither);

        gDevice->CreateTexture2D(&D3D11_TEXTURE2D_DESC {
                                     .Width {GRID},
                                     .Height {GRID},
                                     .MipLevels {u32(bit_width(u32(GRID)))},
                                     .ArraySize {1},
                                     .Format {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
                                     .SampleDesc {.Count {1}},
                                     .BindFlags {D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET},
                                     .MiscFlags {D3D11_RESOURCE_MISC_GENERATE_MIPS},
                                 },
                                 __, &tex);
        gContext->UpdateSubresource(tex, __, __, vec<u8>::load("assets/grid.bin").data(), GRID * 4, __);
        gDevice->CreateShaderResourceView(tex, __, &grid);
        gContext->GenerateMips(grid);

        gDevice->CreateTexture2D(&D3D11_TEXTURE2D_DESC {
                                     .Width {ATLAS_WIDTH},
                                     .Height {ATLAS_HEIGHT},
                                     .MipLevels {1},
                                     .ArraySize {1},
                                     .Format {DXGI_FORMAT_R8_UNORM},
                                     .SampleDesc {.Count {1}},
                                     .BindFlags {D3D11_BIND_SHADER_RESOURCE},
                                 },
                                 &D3D11_SUBRESOURCE_DATA {
                                     .pSysMem {vec<u8>::load("assets/din.bin").data()},
                                     .SysMemPitch {ATLAS_WIDTH},
                                 },
                                 &tex);
        gDevice->CreateShaderResourceView(tex, __, &din);

        return tuple(dither, grid, din);
    }()};

    _ [buffers, level] {[&] {
        _ meshNames {vec<str> {"cube", "sphere"}};
        _ levelNames {vec<str> {"levelLayer0"}};
        // _ levelNames {vec<str> {"triangle"}};

        _ loader {gltf::TinyGLTF()};
        _ loadBinary {[&](str path) {
            _ model {gltf::Model()};
            _ err {str()};
            _ warn {str()};
            _ ok {loader.LoadBinaryFromFile(&model, &err, &warn, path)};
            if (!warn.empty()) print(warn);
            if (!err.empty()) print(err);
            assert(ok);
            return model;
        }};

        _ meshes {map<str, Buffer>()};
        _ level {Level()};

#undef type
        _ getBuffer {[&](gltf::Model glb, int idx) {
            _ a {glb.accessors[idx]};
            _ view {glb.bufferViews[a.bufferView]};
            _ buffer {glb.buffers[view.buffer].data};
            return tuple(a, view, buffer);
        }};
        _ getMesh {[&](gltf::Model glb, int meshIndex = 0) {
            _ res {Buffer()};

            _ mesh {glb.meshes[meshIndex]};
            _ prim {mesh.primitives[0]};

            _ attributes {vec<str> {POSITION, NORMAL, TEXCOORD_0, JOINTS_0, WEIGHTS_0}};
            _ found {vec<str>()};
            for (_ attr : attributes)
            {
                if (prim.attributes.contains(attr))
                {
                    found.push(attr);

                    _ [a, view, buffer] {getBuffer(glb, prim.attributes[attr])};

                    _ buf {ComPtr<ID3D11Buffer>()};
                    _ desc {D3D11_BUFFER_DESC {
                        .ByteWidth {u32(view.byteLength)},
                        .Usage {D3D11_USAGE_IMMUTABLE},
                        .BindFlags {D3D11_BIND_SHADER_RESOURCE},
                        .MiscFlags {D3D11_RESOURCE_MISC_BUFFER_STRUCTURED},
                    }};
                    _ data {D3D11_SUBRESOURCE_DATA {buffer.data() + view.byteOffset}};
                    _ check {[&](int type, int component) {
                        assert(a.type == type && a.componentType == component);
                    }};

                    if (attr == POSITION)
                    {
                        check(TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
                        desc.StructureByteStride = sizeof(vec3);
                    }
                    else if (attr == NORMAL)
                    {
                        check(TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
                        desc.StructureByteStride = sizeof(vec3);
                    }
                    else if (attr == TEXCOORD_0)
                    {
                        check(TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT);
                        desc.StructureByteStride = sizeof(vec2);
                    }
                    else if (attr == JOINTS_0)
                    {
                        check(TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);
                        desc.StructureByteStride = sizeof(ubyte4);
                    }
                    else if (attr == WEIGHTS_0)
                    {
                        check(TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT);
                        desc.StructureByteStride = sizeof(vec4);
                    }

                    gDevice->CreateBuffer(&desc, &data, &buf);
                    gDevice->CreateShaderResourceView(buf, __, &res[attr]);
                }
            }

            _ [a, view, buffer] {getBuffer(glb, prim.indices)};
            assert(a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            gDevice->CreateBuffer(&D3D11_BUFFER_DESC {
                                      .ByteWidth {u32(view.byteLength)},
                                      .Usage {D3D11_USAGE_IMMUTABLE},
                                      .BindFlags {D3D11_BIND_INDEX_BUFFER},
                                  },
                                  &D3D11_SUBRESOURCE_DATA {buffer.data() + view.byteOffset}, &res.index);
            res.indexCount = a.count;
            print(mesh.name, found);
            return res;
        }};
        _ getGeometry {[&](gltf::Model glb, int meshIndex = 0) {
            _ res {Mesh()};

            _ geom {glb.meshes[meshIndex]};
            _ prim {geom.primitives[0]};

            _ [a, view, buffer] {getBuffer(glb, prim.attributes[POSITION])};
            assert(a.type == TINYGLTF_TYPE_VEC3 && a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
            res.pos = vec<vec3>((vec3 *)(buffer.data() + view.byteOffset),
                                (vec3 *)(buffer.data() + view.byteOffset + view.byteLength));
            tie(a, view, buffer) = getBuffer(glb, prim.indices);
            assert(a.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
            res.index = vec<ushort3>((ushort3 *)(buffer.data() + view.byteOffset),
                                     (ushort3 *)(buffer.data() + view.byteOffset + view.byteLength));

            return res;
        }};
#define type typename

        for (_ name : meshNames)
        {
            _ glb {loadBinary(format("assets/{}.glb", name))};
            meshes[name] = getMesh(glb);
        }

        for (_ name : levelNames)
        {
            _ glb {loadBinary(format("assets/{}.glb", name))};
            for (_ i : range(glb.meshes.size()))
            {
                level.buffers.push(getMesh(glb, i));
                level.meshes.push(getGeometry(glb, i));
            }
        }

        return tuple(meshes, level);
    }()};
    _ [phys, player] {[&] {
        _ phys {Physics {.rate {64.0f}}};
        _ player {Player {
            .curPos {5, 2, 0},
            .curVel {0, 10, 0},
            .accel {0, -5.0f, 0},
            .maxGroundSpeed {8.0f},
            .state {Player::Air},
            .outerRadius {0.2f},
            .innerRadius {0.18f},
        }};
        player.pos = player.prevPos = player.curPos;
        player.prevVel = player.curVel;
        return tuple(phys, player);
    }()};
    _ physUpdate {[&] {
        player.prevPos = player.curPos;
        player.prevVel = player.curVel;
        // switch (player.state)
        // {
        //     case Player::Air:
        //         player.curVel += player.accel / phys.rate;
        //         break;
        // }
        _ dir {vec3()};
        if (gKeyDown['W']) dir += player.forward(); // depends on camera orientation
        if (gKeyDown['S']) dir -= player.forward();
        if (gKeyDown['D']) dir += player.right();
        if (gKeyDown['A']) dir -= player.right();
        if (gKeyDown['E']) dir += player.up();
        if (gKeyDown['Q']) dir -= player.up();
        if (!eq(dir, vec3())) dir = normalize(dir);

        player.curVel = dir * player.maxGroundSpeed;

        // start collision routine
        // https://www.peroxide.dk/papers/collision/collision.pdf
        // find the first t such that t*velocity is inside the triangle's "distance field"

        _ velocity {player.curVel / phys.rate};

        _ maxIterations {6}; // set this to max number of faces that can meet at a vertex (in theory, that's how many iterations are necessary, assuming the player doesn't move too quickly)
        _ count {0};

        while (!eq(velocity, vec3()) && count < maxIterations)
        {
            // test collisions against the inner ball. if we collide, then we resolve collision on the outer ball
            // this is to solve the fundamental problem with point-volume testing: it's ill conditioned on the boundary
            _ collided {false};
            _ collisionTime {1.0f};
            _ collisionNormal {vec3()};
            for (_ g : level.meshes)
            {
                for (_ idx : g.index)
                {
                    _ p {g.pos[idx[0]] - player.curPos};
                    _ q {g.pos[idx[1]] - player.curPos};
                    _ r {g.pos[idx[2]] - player.curPos};
                    _ n {normalize(cross(q - p, r - p))};
                    _ c {dot(n, p)}; // plane = {x | dot(n, x) = c}
                    _ nDotV {dot(n, velocity)};

                    _ t1 {(c - player.innerRadius) / nDotV};
                    _ t2 {(c + player.innerRadius) / nDotV};

                    _ handleCollision {[&](float t, vec3 pointOnOriginalPlane) {
                        _ coords {inverse(mat<3, 2>::columns(q - p, r - p)) * (pointOnOriginalPlane - p)};
                        _ a {coords[0]}, b {coords[1]};
                        if (a > 0 && a < 1 && b > 0 && b < 1 && a + b < 1)
                        {
                            collided = true;
                            collisionTime = t;
                            collisionNormal = n;
                        }
                    }};
                    if (t1 > 0 && t1 < collisionTime) // any INF/NAN will be ignored
                    {
                        _ pos {velocity * t1};
                        _ pointOnPlane {pos - n * dot(pos, n) + n * player.innerRadius};
                        handleCollision(t1, pointOnPlane);
                    }
                    if (t2 > 0 && t2 < collisionTime)
                    {
                        _ pos {velocity * t2};
                        _ pointOnPlane {pos - n * dot(pos, n) - n * player.innerRadius};
                        handleCollision(t2, pointOnPlane);
                    }
                    // https://en.wikipedia.org/wiki/Line-cylinder_intersection (line-infinite cylinder intersection)

                    _ handleCylinder {[&](vec3 p, vec3 q) { // infinite cylinder from p to q
                        _ len {length(q - p)};
                        _ a {(q - p) / len};

                        // solve t*velocity on the infinite cylinder
                        _ pCrossA {cross(p, a)};
                        _ vCrossA {cross(velocity, a)};
                        _ numeratorPart {dot(vCrossA, pCrossA)};
                        _ discriminant {dot(vCrossA, vCrossA) * square(player.innerRadius) - square(dot(p, vCrossA))};
                        _ denom {dot(vCrossA, vCrossA)};

                        _ t1 {(numeratorPart + sqrt(discriminant)) / denom};
                        _ t2 {(numeratorPart - sqrt(discriminant)) / denom};

                        _ handleCollision {[&](float t) {
                            _ pos {velocity * t};
                            _ signedDistance {dot(a, pos - p)};
                            if (signedDistance > 0 && signedDistance < len)
                            {
                                collided = true;
                                collisionTime = t;
                                collisionNormal = normalize(pos - (p + a * signedDistance));
                            }
                        }};

                        // any NAN/INF would be ignored
                        if (t1 > 0 && t1 < collisionTime) handleCollision(t1);
                        if (t2 > 0 && t2 < collisionTime) handleCollision(t2);
                    }};
                    handleCylinder(p, q);
                    handleCylinder(q, r);
                    handleCylinder(r, p);

                    _ handleSphere {[&](vec3 p) {
                        _ vDotV {dot(velocity, velocity)};
                        _ pDotP {dot(p, p)};
                        _ vDotP {dot(velocity, p)};

                        _ numeratorPart {vDotP};
                        _ discriminant {square(vDotP) - vDotV * (pDotP - square(player.innerRadius))};
                        _ denom {vDotV};

                        _ t1 {(numeratorPart + sqrt(discriminant)) / denom};
                        _ t2 {(numeratorPart - sqrt(discriminant)) / denom};

                        _ handleCollision {[&](float t) {
                            _ pos {velocity * t};
                            collided = true;
                            collisionTime = t;
                            collisionNormal = normalize(pos - p);
                        }};
                        if (t1 > 0 && t1 < collisionTime) handleCollision(t1);
                        if (t2 > 0 && t2 < collisionTime) handleCollision(t2);
                    }};

                    handleSphere(p);
                    handleSphere(q);
                    handleSphere(r);
                }
            }
            if (collided)
            {
                // both velocity and collisionNormal are nonzero
                // if collided, push the outer radius out of the collision to resolve the previous penetration
                player.curPos += velocity * (collisionTime - (player.outerRadius - player.innerRadius) / length(velocity));

                velocity = (velocity - collisionNormal * dot(velocity, collisionNormal)) * (1 - collisionTime);
            }
            else
            {
                player.curPos += velocity;
                velocity = vec3();
            }
            count++;
        }
    }};

    setTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ShowWindow(gWindow, SW_SHOW); // sends WM_SIZE

    if (gFullscreen)
    {

        SetWindowLong(gWindow, GWL_STYLE, 0);
        ShowWindow(gWindow, SW_SHOWMAXIMIZED);
    }

    _ timer {Timer()};

    while (true)
    {
        _ res {MsgWaitForMultipleObjects(1, &gWaitableObject, false, INFINITE, QS_ALLINPUT)};
        if (res == WAIT_OBJECT_0)
        {

            _ dt {timer.update()};

            player.yaw = gCam.yaw;
            phys.elapsed += dt;
            while (phys.elapsed > 1.0f / phys.rate)
            {
                phys.elapsed -= 1.0f / phys.rate;
                physUpdate();
            }
            player.pos = lerp(player.prevPos, player.curPos, phys.elapsed * phys.rate); // can switch this for cubic spline interpolation
            // ray trace to determine if player is in shadow

            gCam.pos = player.pos - gCam.forward() * gCam.armLength;

            _ dist {vec<float> {gCam.near, 4, 10, 20, 50}}; // cascade distance
            _ aspect {gWidth / gHeight};

            for (_ i : range(CASCADES)) // todo: use https://www-sop.inria.fr/reves/Basilic/2002/SD02/PerspectiveShadowMaps.pdf for even better resolution up close
            {
                _ x1 {dist[i] * tan(gCam.fov / 2)};
                _ y1 {dist[i] * tan(gCam.fov / 2) / aspect};
                _ x2 {dist[i + 1] * tan(gCam.fov / 2)};
                _ y2 {dist[i + 1] * tan(gCam.fov / 2) / aspect};

                _ worldToShadow {shadow.invRotation()};

                _ points {vec<vec3> {
                    {x1, -y1, dist[i]},
                    {x1, y1, dist[i]},
                    {-x1, y1, dist[i]},
                    {-x1, -y1, dist[i]},
                    {x2, -y2, dist[i + 1]},
                    {x2, y2, dist[i + 1]},
                    {-x2, y2, dist[i + 1]},
                    {-x2, -y2, dist[i + 1]},
                }};

                // largest possible length of the view frustum = side length of the shadow map
                // now the shadow texels have a constant size in world space
                _ d {ceil(max(length(points[0] - points[6]), length(points[4] - points[6])))};
                _ T {d / SHADOW_MAP};

                // view space -> world space -> shadow space (each matrix is just a rotation + translation)
                points = fmap(points, [&](vec3 p) { return worldToShadow * vec3(gCam.localToWorld() * vec4(p, 1)); });

                _ X {fmap(points, [](vec3 p) { return p[0]; })};
                _ Y {fmap(points, [](vec3 p) { return p[1]; })};
                _ Z {fmap(points, [](vec3 p) { return p[2]; })};

                _ xMin {rmin(X)}, xMax {rmax(X)}, yMin {rmin(Y)}, yMax {rmax(Y)}, zMin {rmin(Z)}, zMax {rmax(Z)};

                cbPerFrame.shadowWorldToNDC[i] =
                    mat4::affine(mat3::diag(2 / d, 2 / d, -1 / (zMax - zMin)), vec3(0, 0, 1)) *
                    mat4::affine(worldToShadow, -vec3(floor((xMin + xMax) / (2 * T)) * T, floor((yMin + yMax) / (2 * T)) * T, zMin));
            }
            cbPerFrame.camWorldToClip = mat4::perspective(gCam.fov, gWidth / gHeight, gCam.near) * gCam.worldToLocal();
            cbPerFrame.camPos = gCam.pos;
            cbPerFrame.update(gContext);

#pragma region shadow

            setViewport(SHADOW_MAP, SHADOW_MAP);
            setDepthState("greater");
            setRasterState("shadow");
            setVS("shadow");
            clearPS();

            for (_ i : range(CASCADES))
            {
                setRenderTarget(__, dsvShadows[i]); // note: setRenderTarget behaves like "array = {...}" as opposed to other bindings, where "array[input range] = {...}"
                clearDepth(dsvShadows[i], 0);
                cbShadow.index = i;
                cbShadow.update(gContext);

                cbModel.world = mat4::id();
                cbModel.update(gContext);
                for (_ buf : level.buffers)
                {
                    setVSResources(0, {buf[POSITION]});
                    setIndexBuffer(buf.index);
                    drawIndexed(buf.indexCount);
                }

                cbModel.world = player.localToWorld();
                cbModel.update(gContext);
                setVSResources(0, {buffers["sphere"][POSITION]});
                setIndexBuffer(buffers["sphere"].index);
                drawIndexed(buffers["sphere"].indexCount);
            }

#pragma endregion

#pragma region skybox
            setRenderTarget({gRtvWindow}, gDsvWindow);
            setViewport(gWidth, gHeight);

            setDepthState("disabled");
            setRasterState("noCull");

            setVS("pos");
            setPS("skybox");

            cbModel.world = mat4::translate(gCam.pos);
            cbModel.update(gContext);
            setVSResources(0, {buffers["cube"][POSITION]});
            setPSResources(0, {srvDither});
            setIndexBuffer(buffers["cube"].index);
            drawIndexed(buffers["cube"].indexCount);

#pragma endregion

#pragma region scene

            clearDepth(gDsvWindow, 0);
            setDepthState("greater");
            clearRasterState();
            // setRasterState("noCull");

            setVS("posNormalUV");
            setPS("lambertReceiveShadows");
            setPSResources(0, {srvGrid});
            setPSResources(1, srvShadows);
            setPSSamplers(0, {sampler["grid"], sampler["cmpGreater"]});

            cbModel.world = mat4::id();
            cbModel.update(gContext);
            for (_ buf : level.buffers)
            {
                setVSResources(0, {buf[POSITION], buf[NORMAL], buf[TEXCOORD_0]});
                setIndexBuffer(buf.index);
                drawIndexed(buf.indexCount);
            }

            setVS("clip");
            setPS("solid");
            setRasterState("wireframe");

            setVSResources(0, {buffers["sphere"][POSITION]});
            setIndexBuffer(buffers["sphere"].index);

            cbModel.world = player.localToWorld();
            cbModel.update(gContext);
            drawIndexed(buffers["sphere"].indexCount);

            // cbModel.world = mat4::translate(player.prevPos);
            // cbModel.update(gContext);
            // drawIndexed(sphere.indexCount);
            // cbModel.world = mat4::translate(player.curPos);
            // cbModel.update(gContext);
            // drawIndexed(sphere.indexCount);

            clearPSResources(1, CASCADES);
#pragma endregion

#pragma region axis
            setTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
            setVS("axis");
            setPS("color");
            draw(6);
            setTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

#pragma region text

            // drawing functions rely on necessary state being set up before calling them
            _ drawText {[&](str s, vec2 pos) {
                _ cur {vec2(pos[0], gHeight - pos[1])};
                for (_ c : s)
                {
                    // there are 3 degrees of freedom for the layout of a single character
                    // due to text layout purposes: we assume that the space between two characters is a function of
                    // both characters. in particular, we define the spacing as (right side of c1) + (left side of c2)
                    cbGlyph.topLeft = glyphMetrics[c].topLeft;
                    cbGlyph.scale = glyphMetrics[c].scale;

                    cbGlyph.pos[0] = cur[0] + glyphMetrics[c].offsetX;
                    cbGlyph.pos[1] = cur[1] - glyphMetrics[c].scale[1] + glyphMetrics[c].offsetY;

                    cbGlyph.update(gContext);
                    gContext->Draw(6, 0);
                    cur[0] += glyphMetrics[c].advance;
                }
            }};
            setVS("font");
            setPS("font");
            setBlendState("alpha");
            setPSSamplers(0, {sampler["font"]});
            setPSResources(0, {srvDin});
            setDepthState("disabled");
            setRasterState("noCull");
            cbGlyph.color = vec4(1, 1, 1, 1);
            drawText("Hello, World!", vec2(100, 100));

            clearBlendState();

#pragma endregion

            gSwapChain->Present(1, 0);
            // FrameMark;
        }
        else
        {
            _ msg {MSG()};
            while (PeekMessage(&msg, __, __, __, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) ExitProcess(0);
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    return 0;
}
