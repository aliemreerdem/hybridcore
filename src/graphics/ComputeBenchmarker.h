#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>

using Microsoft::WRL::ComPtr;

namespace graphics {

struct GpuTelemetry {
    float       temperatureC    = 0.0f;   // GPU die temperature (Celsius)
    float       coreClockMhz    = 0.0f;   // GPU core clock (MHz)
    float       memClockMhz     = 0.0f;   // VRAM clock (MHz)
    uint64_t    vramUsedMB      = 0;       // VRAM currently used (MB)
    uint64_t    vramTotalMB     = 0;       // VRAM budget (MB)
    float       vramUsagePercent= 0.0f;    // VRAM usage %
    float       gpuLoadPercent  = 0.0f;    // Estimated GPU load (based on throughput)
};

class ComputeBenchmarker {
public:
    ComputeBenchmarker();
    ~ComputeBenchmarker();

    bool Initialize(UINT adapterIndex);
    void DispatchWorkload();
    void CheckForShaderUpdates();

    // Telemetry
    void UpdateTelemetry();
    const GpuTelemetry& GetTelemetry() const { return m_telemetry; }
    std::wstring GetAdapterName() const { return m_adapterName; }

private:
    bool LoadShader(const std::wstring& path);
    bool CompileAndSwapShader(const std::wstring& hlslPath);
    void QueryGpuTemperature();
    void QueryVramUsage();

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<ID3D11ComputeShader> m_computeShader;
    std::mutex m_shaderMutex;
    
    // Shader write target
    ComPtr<ID3D11Buffer> m_uavBuffer;
    ComPtr<ID3D11UnorderedAccessView> m_uav;

    FILETIME m_lastShaderWriteTime;
    std::wstring m_shaderSourcePath;

    // Telemetry state
    GpuTelemetry m_telemetry;
    std::wstring m_adapterName;
    UINT m_adapterIndex = 0;
    LUID m_adapterLuid = {};
    uint64_t m_dedicatedVramBytes = 0;

    // D3DKMT handle for kernel queries
    UINT m_d3dkmtAdapterHandle = 0;
    bool m_d3dkmtAvailable = false;

    std::chrono::steady_clock::time_point m_lastTelemetryUpdate;
};

} // namespace graphics
