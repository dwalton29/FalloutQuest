#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN64)
#error Fallout 3 is a 32-bit process. Build this proxy with a Win32/x86 target.
#endif

namespace {

constexpr std::size_t kMaxTextureStages = 8;
constexpr std::size_t kTextureHashBudget = 64 * 1024;

HMODULE gRealD3D9 = nullptr;
std::mutex gLogMutex;
FILE* gLog = nullptr;
std::atomic<bool> gCaptureActive{false};
std::atomic<std::uint64_t> gFrameId{0};
std::atomic<std::uint64_t> gDrawId{0};

struct ShaderInfo {
    std::uint64_t hash = 0;
    UINT byteCount = 0;
    DWORD versionToken = 0;
};

struct TextureInfo {
    std::string description;
};

std::mutex gMetadataMutex;
std::unordered_map<void*, ShaderInfo> gShaders;
std::unordered_map<void*, TextureInfo> gTextures;

std::uint64_t Fnv1a64(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

std::wstring LogPath() {
    wchar_t exePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return L"Fallout3D3D9.log";
    }
    std::wstring path(exePath, length);
    const auto slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        path.resize(slash + 1);
    } else {
        path.clear();
    }
    path += L"Fallout3D3D9.log";
    return path;
}

void EnsureLogOpenLocked() {
    if (gLog) return;
    const std::wstring path = LogPath();
    gLog = _wfopen(path.c_str(), L"wt");
    if (!gLog) return;
    setvbuf(gLog, nullptr, _IOLBF, 0);

    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::fprintf(gLog,
                 "FQ_D3D9_LOGGER version=1 pid=%lu time=%04u-%02u-%02uT%02u:%02u:%02u.%03u hotkey=F10 capture=next_full_frame\n",
                 GetCurrentProcessId(), st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void Log(const char* format, ...) {
    std::lock_guard<std::mutex> lock(gLogMutex);
    EnsureLogOpenLocked();
    if (!gLog) return;
    va_list args;
    va_start(args, format);
    std::vfprintf(gLog, format, args);
    va_end(args);
    std::fputc('\n', gLog);
}

void FlushLog() {
    std::lock_guard<std::mutex> lock(gLogMutex);
    if (gLog) std::fflush(gLog);
}

HMODULE RealD3D9() {
    if (gRealD3D9) return gRealD3D9;
    wchar_t systemDir[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(systemDir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH - 10) return nullptr;
    std::wstring path(systemDir, length);
    path += L"\\d3d9.dll";
    gRealD3D9 = LoadLibraryW(path.c_str());
    return gRealD3D9;
}

template <typename T>
T RealProc(const char* name) {
    HMODULE module = RealD3D9();
    return module ? reinterpret_cast<T>(GetProcAddress(module, name)) : nullptr;
}

template <typename Fn>
bool PatchVtable(void* object, std::size_t index, Fn hook, Fn* original) {
    if (!object || !original) return false;
    auto*** objectAsVtable = reinterpret_cast<void***>(object);
    void** vtable = *objectAsVtable;
    if (!vtable) return false;

    void* hookPtr = reinterpret_cast<void*>(hook);
    if (vtable[index] == hookPtr) return true;

    DWORD oldProtect = 0;
    if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    if (!*original) {
        *original = reinterpret_cast<Fn>(vtable[index]);
    }
    vtable[index] = hookPtr;
    DWORD ignored = 0;
    VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), &vtable[index], sizeof(void*));
    return true;
}

ShaderInfo ReadShaderInfo(IDirect3DVertexShader9* shader) {
    ShaderInfo info{};
    if (!shader) return info;
    UINT size = 0;
    if (FAILED(shader->GetFunction(nullptr, &size)) || size == 0) return info;
    std::vector<std::uint8_t> bytes(size);
    if (FAILED(shader->GetFunction(bytes.data(), &size)) || size == 0) return info;
    info.byteCount = size;
    info.hash = Fnv1a64(bytes.data(), size);
    if (size >= sizeof(DWORD)) std::memcpy(&info.versionToken, bytes.data(), sizeof(DWORD));
    return info;
}

ShaderInfo ReadShaderInfo(IDirect3DPixelShader9* shader) {
    ShaderInfo info{};
    if (!shader) return info;
    UINT size = 0;
    if (FAILED(shader->GetFunction(nullptr, &size)) || size == 0) return info;
    std::vector<std::uint8_t> bytes(size);
    if (FAILED(shader->GetFunction(bytes.data(), &size)) || size == 0) return info;
    info.byteCount = size;
    info.hash = Fnv1a64(bytes.data(), size);
    if (size >= sizeof(DWORD)) std::memcpy(&info.versionToken, bytes.data(), sizeof(DWORD));
    return info;
}

template <typename T>
ShaderInfo GetShaderInfo(T* shader) {
    if (!shader) return {};
    {
        std::lock_guard<std::mutex> lock(gMetadataMutex);
        const auto it = gShaders.find(shader);
        if (it != gShaders.end()) return it->second;
    }
    const ShaderInfo info = ReadShaderInfo(shader);
    {
        std::lock_guard<std::mutex> lock(gMetadataMutex);
        gShaders[shader] = info;
    }
    return info;
}

const char* FormatName(D3DFORMAT format) {
    switch (format) {
        case D3DFMT_A8R8G8B8: return "A8R8G8B8";
        case D3DFMT_X8R8G8B8: return "X8R8G8B8";
        case D3DFMT_R5G6B5: return "R5G6B5";
        case D3DFMT_A1R5G5B5: return "A1R5G5B5";
        case D3DFMT_A4R4G4B4: return "A4R4G4B4";
        case D3DFMT_A8: return "A8";
        case D3DFMT_L8: return "L8";
        case D3DFMT_A8L8: return "A8L8";
        case D3DFMT_DXT1: return "DXT1";
        case D3DFMT_DXT2: return "DXT2";
        case D3DFMT_DXT3: return "DXT3";
        case D3DFMT_DXT4: return "DXT4";
        case D3DFMT_DXT5: return "DXT5";
        default: return "OTHER";
    }
}

std::string DescribeTexture(IDirect3DBaseTexture9* base) {
    if (!base) return "null";
    {
        std::lock_guard<std::mutex> lock(gMetadataMutex);
        const auto it = gTextures.find(base);
        if (it != gTextures.end()) return it->second.description;
    }

    char buffer[512]{};
    const D3DRESOURCETYPE type = base->GetType();
    if (type != D3DRTYPE_TEXTURE) {
        std::snprintf(buffer, sizeof(buffer), "ptr=%p type=%d", base, static_cast<int>(type));
    } else {
        auto* texture = static_cast<IDirect3DTexture9*>(base);
        D3DSURFACE_DESC desc{};
        if (FAILED(texture->GetLevelDesc(0, &desc))) {
            std::snprintf(buffer, sizeof(buffer), "ptr=%p type=2D desc=unavailable", base);
        } else {
            bool hashValid = false;
            std::uint64_t hash = 0;
            std::size_t hashedBytes = 0;
            D3DLOCKED_RECT locked{};
            if (SUCCEEDED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)) && locked.pBits && locked.Pitch != 0) {
                const bool blockCompressed = desc.Format == D3DFMT_DXT1 || desc.Format == D3DFMT_DXT2 ||
                                             desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT4 ||
                                             desc.Format == D3DFMT_DXT5;
                const UINT rows = blockCompressed ? std::max<UINT>(1, (desc.Height + 3) / 4) : desc.Height;
                const std::size_t pitch = static_cast<std::size_t>(locked.Pitch < 0 ? -locked.Pitch : locked.Pitch);
                std::uint64_t running = 14695981039346656037ull;
                const auto* row = static_cast<const std::uint8_t*>(locked.pBits);
                for (UINT y = 0; y < rows && hashedBytes < kTextureHashBudget; ++y) {
                    const std::size_t take = std::min<std::size_t>(pitch, kTextureHashBudget - hashedBytes);
                    for (std::size_t i = 0; i < take; ++i) {
                        running ^= row[i];
                        running *= 1099511628211ull;
                    }
                    hashedBytes += take;
                    row += locked.Pitch;
                }
                hash = running;
                hashValid = hashedBytes > 0;
                texture->UnlockRect(0);
            }
            std::snprintf(buffer, sizeof(buffer),
                          "ptr=%p type=2D w=%u h=%u levels=%u fmt=%s(%d) usage=0x%08lx pool=%d hash=%s%016llx bytes=%zu",
                          base, desc.Width, desc.Height, texture->GetLevelCount(),
                          FormatName(desc.Format), static_cast<int>(desc.Format),
                          static_cast<unsigned long>(desc.Usage), static_cast<int>(desc.Pool),
                          hashValid ? "" : "NA/",
                          static_cast<unsigned long long>(hash), hashedBytes);
        }
    }

    TextureInfo info{buffer};
    {
        std::lock_guard<std::mutex> lock(gMetadataMutex);
        gTextures[base] = info;
    }
    return info.description;
}

const char* PrimitiveName(D3DPRIMITIVETYPE primitive) {
    switch (primitive) {
        case D3DPT_POINTLIST: return "POINTLIST";
        case D3DPT_LINELIST: return "LINELIST";
        case D3DPT_LINESTRIP: return "LINESTRIP";
        case D3DPT_TRIANGLELIST: return "TRIANGLELIST";
        case D3DPT_TRIANGLESTRIP: return "TRIANGLESTRIP";
        case D3DPT_TRIANGLEFAN: return "TRIANGLEFAN";
        default: return "UNKNOWN";
    }
}

void LogFloatConstants(const char* label, std::uint64_t draw, const float* values, UINT count) {
    for (UINT reg = 0; reg < count; ++reg) {
        const float* v = values + reg * 4;
        if (v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f && v[3] == 0.0f) continue;
        Log("%s draw=%llu r=%u v=%.9g,%.9g,%.9g,%.9g",
            label, static_cast<unsigned long long>(draw), reg,
            v[0], v[1], v[2], v[3]);
    }
}

void LogRenderState(IDirect3DDevice9* device, std::uint64_t draw, D3DRENDERSTATETYPE state, const char* name) {
    DWORD value = 0;
    if (SUCCEEDED(device->GetRenderState(state, &value))) {
        Log("RS draw=%llu name=%s id=%d value=%lu hex=0x%08lx",
            static_cast<unsigned long long>(draw), name, static_cast<int>(state),
            static_cast<unsigned long>(value), static_cast<unsigned long>(value));
    }
}

void LogSamplerState(IDirect3DDevice9* device, std::uint64_t draw, DWORD sampler,
                     D3DSAMPLERSTATETYPE state, const char* name) {
    DWORD value = 0;
    if (SUCCEEDED(device->GetSamplerState(sampler, state, &value))) {
        Log("SS draw=%llu sampler=%lu name=%s id=%d value=%lu hex=0x%08lx",
            static_cast<unsigned long long>(draw), static_cast<unsigned long>(sampler),
            name, static_cast<int>(state), static_cast<unsigned long>(value),
            static_cast<unsigned long>(value));
    }
}

void DumpDrawState(IDirect3DDevice9* device, const char* kind, D3DPRIMITIVETYPE primitive,
                   INT baseVertex, UINT minVertex, UINT numVertices, UINT startIndex,
                   UINT primitiveCount) {
    if (!gCaptureActive.load(std::memory_order_relaxed)) return;

    const std::uint64_t draw = ++gDrawId;
    IDirect3DPixelShader9* ps = nullptr;
    IDirect3DVertexShader9* vs = nullptr;
    device->GetPixelShader(&ps);
    device->GetVertexShader(&vs);
    const ShaderInfo psInfo = GetShaderInfo(ps);
    const ShaderInfo vsInfo = GetShaderInfo(vs);

    Log("DRAW id=%llu frame=%llu kind=%s primitive=%s(%d) baseVertex=%d minVertex=%u numVertices=%u startIndex=%u primitiveCount=%u ps=%016llx psBytes=%u psVer=0x%08lx vs=%016llx vsBytes=%u vsVer=0x%08lx",
        static_cast<unsigned long long>(draw), static_cast<unsigned long long>(gFrameId.load()),
        kind, PrimitiveName(primitive), static_cast<int>(primitive), baseVertex, minVertex,
        numVertices, startIndex, primitiveCount,
        static_cast<unsigned long long>(psInfo.hash), psInfo.byteCount,
        static_cast<unsigned long>(psInfo.versionToken),
        static_cast<unsigned long long>(vsInfo.hash), vsInfo.byteCount,
        static_cast<unsigned long>(vsInfo.versionToken));

    for (DWORD stage = 0; stage < kMaxTextureStages; ++stage) {
        IDirect3DBaseTexture9* texture = nullptr;
        if (SUCCEEDED(device->GetTexture(stage, &texture))) {
            const std::string description = DescribeTexture(texture);
            Log("TEX draw=%llu stage=%lu %s",
                static_cast<unsigned long long>(draw), static_cast<unsigned long>(stage),
                description.c_str());
            if (texture) texture->Release();
        }
    }

    LogRenderState(device, draw, D3DRS_SRGBWRITEENABLE, "SRGBWRITEENABLE");
    LogRenderState(device, draw, D3DRS_ZENABLE, "ZENABLE");
    LogRenderState(device, draw, D3DRS_ZWRITEENABLE, "ZWRITEENABLE");
    LogRenderState(device, draw, D3DRS_CULLMODE, "CULLMODE");
    LogRenderState(device, draw, D3DRS_ALPHABLENDENABLE, "ALPHABLENDENABLE");
    LogRenderState(device, draw, D3DRS_SRCBLEND, "SRCBLEND");
    LogRenderState(device, draw, D3DRS_DESTBLEND, "DESTBLEND");
    LogRenderState(device, draw, D3DRS_FOGENABLE, "FOGENABLE");
    LogRenderState(device, draw, D3DRS_LIGHTING, "LIGHTING");
    LogRenderState(device, draw, D3DRS_COLORWRITEENABLE, "COLORWRITEENABLE");

    for (DWORD sampler = 0; sampler < kMaxTextureStages; ++sampler) {
        LogSamplerState(device, draw, sampler, D3DSAMP_SRGBTEXTURE, "SRGBTEXTURE");
        LogSamplerState(device, draw, sampler, D3DSAMP_MINFILTER, "MINFILTER");
        LogSamplerState(device, draw, sampler, D3DSAMP_MAGFILTER, "MAGFILTER");
        LogSamplerState(device, draw, sampler, D3DSAMP_MIPFILTER, "MIPFILTER");
        LogSamplerState(device, draw, sampler, D3DSAMP_ADDRESSU, "ADDRESSU");
        LogSamplerState(device, draw, sampler, D3DSAMP_ADDRESSV, "ADDRESSV");
    }

    float psConstants[32 * 4]{};
    if (SUCCEEDED(device->GetPixelShaderConstantF(0, psConstants, 32))) {
        LogFloatConstants("PSF", draw, psConstants, 32);
    }

    D3DCAPS9 caps{};
    UINT vsRegisters = 256;
    if (SUCCEEDED(device->GetDeviceCaps(&caps)) && caps.MaxVertexShaderConst > 0) {
        vsRegisters = std::min<UINT>(caps.MaxVertexShaderConst, 256);
    }
    std::vector<float> vsConstants(static_cast<std::size_t>(vsRegisters) * 4);
    if (vsRegisters > 0 && SUCCEEDED(device->GetVertexShaderConstantF(0, vsConstants.data(), vsRegisters))) {
        LogFloatConstants("VSF", draw, vsConstants.data(), vsRegisters);
    }

    if (ps) ps->Release();
    if (vs) vs->Release();
}

using CreateDeviceFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
                                                     D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using PresentFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using DrawPrimitiveFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
using DrawIndexedPrimitiveFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
using DrawPrimitiveUPFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, const void*, UINT);
using DrawIndexedPrimitiveUPFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT, UINT,
                                                               const void*, D3DFORMAT, const void*, UINT);
using CreateVertexShaderFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, const DWORD*, IDirect3DVertexShader9**);
using CreatePixelShaderFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, const DWORD*, IDirect3DPixelShader9**);

CreateDeviceFn gCreateDevice = nullptr;
PresentFn gPresent = nullptr;
DrawPrimitiveFn gDrawPrimitive = nullptr;
DrawIndexedPrimitiveFn gDrawIndexedPrimitive = nullptr;
DrawPrimitiveUPFn gDrawPrimitiveUP = nullptr;
DrawIndexedPrimitiveUPFn gDrawIndexedPrimitiveUP = nullptr;
CreateVertexShaderFn gCreateVertexShader = nullptr;
CreatePixelShaderFn gCreatePixelShader = nullptr;

HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9* device, const RECT* source, const RECT* dest,
                                      HWND window, const RGNDATA* dirty) {
    if (gCaptureActive.exchange(false)) {
        Log("FRAME_END id=%llu draws=%llu",
            static_cast<unsigned long long>(gFrameId.load()),
            static_cast<unsigned long long>(gDrawId.load()));
        FlushLog();
    }

    const HRESULT result = gPresent(device, source, dest, window, dirty);

    if (GetAsyncKeyState(VK_F10) & 1) {
        const std::uint64_t frame = ++gFrameId;
        gDrawId = 0;
        gCaptureActive = true;
        Log("FRAME_BEGIN id=%llu trigger=F10 note=all_state_is_queried_at_each_draw",
            static_cast<unsigned long long>(frame));
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookDrawPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE primitive,
                                            UINT startVertex, UINT primitiveCount) {
    DumpDrawState(device, "DrawPrimitive", primitive, 0, startVertex, 0, 0, primitiveCount);
    return gDrawPrimitive(device, primitive, startVertex, primitiveCount);
}

HRESULT STDMETHODCALLTYPE HookDrawIndexedPrimitive(IDirect3DDevice9* device, D3DPRIMITIVETYPE primitive,
                                                   INT baseVertex, UINT minVertex, UINT numVertices,
                                                   UINT startIndex, UINT primitiveCount) {
    DumpDrawState(device, "DrawIndexedPrimitive", primitive, baseVertex, minVertex,
                  numVertices, startIndex, primitiveCount);
    return gDrawIndexedPrimitive(device, primitive, baseVertex, minVertex, numVertices, startIndex, primitiveCount);
}

HRESULT STDMETHODCALLTYPE HookDrawPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE primitive,
                                              UINT primitiveCount, const void* data, UINT stride) {
    DumpDrawState(device, "DrawPrimitiveUP", primitive, 0, 0, 0, 0, primitiveCount);
    return gDrawPrimitiveUP(device, primitive, primitiveCount, data, stride);
}

HRESULT STDMETHODCALLTYPE HookDrawIndexedPrimitiveUP(IDirect3DDevice9* device, D3DPRIMITIVETYPE primitive,
                                                     UINT minVertex, UINT numVertices, UINT primitiveCount,
                                                     const void* indexData, D3DFORMAT indexFormat,
                                                     const void* vertexData, UINT stride) {
    DumpDrawState(device, "DrawIndexedPrimitiveUP", primitive, 0, minVertex,
                  numVertices, 0, primitiveCount);
    return gDrawIndexedPrimitiveUP(device, primitive, minVertex, numVertices, primitiveCount,
                                   indexData, indexFormat, vertexData, stride);
}

HRESULT STDMETHODCALLTYPE HookCreateVertexShader(IDirect3DDevice9* device, const DWORD* function,
                                                 IDirect3DVertexShader9** shader) {
    const HRESULT result = gCreateVertexShader(device, function, shader);
    if (SUCCEEDED(result) && shader && *shader) {
        const ShaderInfo info = ReadShaderInfo(*shader);
        std::lock_guard<std::mutex> lock(gMetadataMutex);
        gShaders[*shader] = info;
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreatePixelShader(IDirect3DDevice9* device, const DWORD* function,
                                                IDirect3DPixelShader9** shader) {
    const HRESULT result = gCreatePixelShader(device, function, shader);
    if (SUCCEEDED(result) && shader && *shader) {
        const ShaderInfo info = ReadShaderInfo(*shader);
        std::lock_guard<std::mutex> lock(gMetadataMutex);
        gShaders[*shader] = info;
    }
    return result;
}

void PatchDevice(IDirect3DDevice9* device) {
    if (!device) return;
    bool ok = true;
    ok &= PatchVtable(device, 17, &HookPresent, &gPresent);
    ok &= PatchVtable(device, 81, &HookDrawPrimitive, &gDrawPrimitive);
    ok &= PatchVtable(device, 82, &HookDrawIndexedPrimitive, &gDrawIndexedPrimitive);
    ok &= PatchVtable(device, 83, &HookDrawPrimitiveUP, &gDrawPrimitiveUP);
    ok &= PatchVtable(device, 84, &HookDrawIndexedPrimitiveUP, &gDrawIndexedPrimitiveUP);
    ok &= PatchVtable(device, 91, &HookCreateVertexShader, &gCreateVertexShader);
    ok &= PatchVtable(device, 106, &HookCreatePixelShader, &gCreatePixelShader);

    D3DCAPS9 caps{};
    device->GetDeviceCaps(&caps);
    Log("DEVICE_PATCHED ok=%d ptr=%p adapterOrdinal=%u deviceType=%d vs=0x%08lx ps=0x%08lx maxVSConst=%lu",
        ok ? 1 : 0, device, caps.AdapterOrdinal, static_cast<int>(caps.DeviceType),
        static_cast<unsigned long>(caps.VertexShaderVersion),
        static_cast<unsigned long>(caps.PixelShaderVersion),
        static_cast<unsigned long>(caps.MaxVertexShaderConst));
}

HRESULT STDMETHODCALLTYPE HookCreateDevice(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE deviceType,
                                           HWND focusWindow, DWORD behavior,
                                           D3DPRESENT_PARAMETERS* params, IDirect3DDevice9** device) {
    const HRESULT result = gCreateDevice(d3d, adapter, deviceType, focusWindow, behavior, params, device);
    if (SUCCEEDED(result) && device && *device) {
        PatchDevice(*device);
    } else {
        Log("CREATE_DEVICE_FAILED hr=0x%08lx adapter=%u deviceType=%d behavior=0x%08lx",
            static_cast<unsigned long>(result), adapter, static_cast<int>(deviceType),
            static_cast<unsigned long>(behavior));
    }
    return result;
}

void PatchDirect3D9(IDirect3D9* d3d) {
    if (!d3d) return;
    const bool ok = PatchVtable(d3d, 16, &HookCreateDevice, &gCreateDevice);
    Log("D3D9_PATCHED ok=%d ptr=%p", ok ? 1 : 0, d3d);
}

} // namespace

extern "C" __declspec(dllexport) IDirect3D9* WINAPI Direct3DCreate9(UINT sdkVersion) {
    using Fn = IDirect3D9* (WINAPI*)(UINT);
    Fn real = RealProc<Fn>("Direct3DCreate9");
    if (!real) return nullptr;
    IDirect3D9* d3d = real(sdkVersion);
    PatchDirect3D9(d3d);
    return d3d;
}

extern "C" __declspec(dllexport) HRESULT WINAPI Direct3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** out) {
    using Fn = HRESULT (WINAPI*)(UINT, IDirect3D9Ex**);
    Fn real = RealProc<Fn>("Direct3DCreate9Ex");
    if (!real) return D3DERR_NOTAVAILABLE;
    const HRESULT result = real(sdkVersion, out);
    if (SUCCEEDED(result) && out && *out) {
        PatchDirect3D9(static_cast<IDirect3D9*>(*out));
    }
    return result;
}

extern "C" __declspec(dllexport) void WINAPI D3DPERF_SetOptions(DWORD options) {
    using Fn = void (WINAPI*)(DWORD);
    Fn real = RealProc<Fn>("D3DPERF_SetOptions");
    if (real) real(options);
}

extern "C" __declspec(dllexport) int WINAPI D3DPERF_BeginEvent(D3DCOLOR color, LPCWSTR name) {
    using Fn = int (WINAPI*)(D3DCOLOR, LPCWSTR);
    Fn real = RealProc<Fn>("D3DPERF_BeginEvent");
    return real ? real(color, name) : -1;
}

extern "C" __declspec(dllexport) int WINAPI D3DPERF_EndEvent() {
    using Fn = int (WINAPI*)();
    Fn real = RealProc<Fn>("D3DPERF_EndEvent");
    return real ? real() : -1;
}

extern "C" __declspec(dllexport) DWORD WINAPI D3DPERF_GetStatus() {
    using Fn = DWORD (WINAPI*)();
    Fn real = RealProc<Fn>("D3DPERF_GetStatus");
    return real ? real() : 0;
}

extern "C" __declspec(dllexport) BOOL WINAPI D3DPERF_QueryRepeatFrame() {
    using Fn = BOOL (WINAPI*)();
    Fn real = RealProc<Fn>("D3DPERF_QueryRepeatFrame");
    return real ? real() : FALSE;
}

extern "C" __declspec(dllexport) void WINAPI D3DPERF_SetMarker(D3DCOLOR color, LPCWSTR name) {
    using Fn = void (WINAPI*)(D3DCOLOR, LPCWSTR);
    Fn real = RealProc<Fn>("D3DPERF_SetMarker");
    if (real) real(color, name);
}

extern "C" __declspec(dllexport) void WINAPI D3DPERF_SetRegion(D3DCOLOR color, LPCWSTR name) {
    using Fn = void (WINAPI*)(D3DCOLOR, LPCWSTR);
    Fn real = RealProc<Fn>("D3DPERF_SetRegion");
    if (real) real(color, name);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
