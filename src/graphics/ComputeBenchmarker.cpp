#include "ComputeBenchmarker.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <d3dcompiler.h>
#include <dxgi1_4.h>

// D3DKMT Kernel Mode Transport — same API Windows Task Manager uses for GPU telemetry
#include <d3dkmthk.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "gdi32.lib")

namespace graphics {

ComputeBenchmarker::ComputeBenchmarker() {
    m_lastShaderWriteTime.dwLowDateTime = 0;
    m_lastShaderWriteTime.dwHighDateTime = 0;
    m_shaderSourcePath = L"src\\shaders\\compute.hlsl";
    m_lastTelemetryUpdate = std::chrono::steady_clock::now();
}

ComputeBenchmarker::~ComputeBenchmarker() {
    if (m_d3dkmtAvailable && m_d3dkmtAdapterHandle) {
        D3DKMT_CLOSEADAPTER closeAdapter = {};
        closeAdapter.hAdapter = m_d3dkmtAdapterHandle;
        D3DKMTCloseAdapter(&closeAdapter);
    }
}

bool ComputeBenchmarker::Initialize(UINT adapterIndex) {
    m_adapterIndex = adapterIndex;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(factory->EnumAdapters1(adapterIndex, &adapter))) {
        std::cerr << "[ComputeBenchmarker] Adapter " << adapterIndex << " could not be obtained!" << std::endl;
        return false;
    }

    // Grab adapter description for naming and LUID for D3DKMT
    DXGI_ADAPTER_DESC1 adapterDesc;
    adapter->GetDesc1(&adapterDesc);
    m_adapterName = adapterDesc.Description;
    m_adapterLuid = adapterDesc.AdapterLuid;
    m_dedicatedVramBytes = adapterDesc.DedicatedVideoMemory;
    m_telemetry.vramTotalMB = m_dedicatedVramBytes / (1024 * 1024);

    // Create D3D11 device on target adapter
    HRESULT hr = D3D11CreateDevice(
        adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        createDeviceFlags,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &m_device,
        nullptr,
        &m_context
    );

    if (FAILED(hr)) {
        std::cerr << "[ComputeBenchmarker] D3D11 Device creation failed!" << std::endl;
        return false;
    }

    // Open D3DKMT adapter handle for kernel-level temperature/clock queries
    D3DKMT_OPENADAPTERFROMLUID openAdapter = {};
    openAdapter.AdapterLuid = m_adapterLuid;
    if (SUCCEEDED(D3DKMTOpenAdapterFromLuid(&openAdapter))) {
        m_d3dkmtAdapterHandle = openAdapter.hAdapter;
        m_d3dkmtAvailable = true;
    }

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(m_shaderSourcePath.c_str(), GetFileExInfoStandard, &fileInfo)) {
        m_lastShaderWriteTime = fileInfo.ftLastWriteTime;
        CompileAndSwapShader(m_shaderSourcePath);
    } else {
        std::cerr << "[ComputeBenchmarker] Warning: compute.hlsl source file missing! Falling back to cached payload." << std::endl;
        LoadShader(L"bin\\shaders\\compute.cso");
    }

    // UAV buffer for compute shader output
    D3D11_BUFFER_DESC desc = {};
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    desc.ByteWidth = sizeof(float) * 1024 * 256; 
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(float);

    hr = m_device->CreateBuffer(&desc, nullptr, &m_uavBuffer);
    if (FAILED(hr)) return false;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = 1024 * 256;

    hr = m_device->CreateUnorderedAccessView(m_uavBuffer.Get(), &uavDesc, &m_uav);
    if (FAILED(hr)) return false;

    std::cout << "[ComputeBenchmarker] D3D11 Compute Hardware Ready! Linked to GPU." << std::endl;
    return true;
}

void ComputeBenchmarker::QueryGpuTemperature() {
    if (!m_d3dkmtAvailable) return;

    // Query adapter-level perf data (temperature, memory clock, fan, power)
    D3DKMT_ADAPTER_PERFDATA perfData = {};
    perfData.PhysicalAdapterIndex = 0;
    D3DKMT_QUERYADAPTERINFO queryInfo = {};
    queryInfo.hAdapter = m_d3dkmtAdapterHandle;
    queryInfo.Type = KMTQAITYPE_ADAPTERPERFDATA;
    queryInfo.pPrivateDriverData = &perfData;
    queryInfo.PrivateDriverDataSize = sizeof(perfData);

    NTSTATUS status = D3DKMTQueryAdapterInfo(&queryInfo);
    if (status == 0) { // STATUS_SUCCESS
        // Temperature is reported in deci-Celsius (1 = 0.1C)
        m_telemetry.temperatureC = static_cast<float>(perfData.Temperature) / 10.0f;
        // Memory frequency in Hz -> MHz
        m_telemetry.memClockMhz  = static_cast<float>(perfData.MemoryFrequency) / 1000000.0f;
    }

    // Query node-level perf data (core clock from engine node 0)
    D3DKMT_NODE_PERFDATA nodeData = {};
    nodeData.NodeOrdinal = 0;
    nodeData.PhysicalAdapterIndex = 0;
    D3DKMT_QUERYADAPTERINFO nodeQuery = {};
    nodeQuery.hAdapter = m_d3dkmtAdapterHandle;
    nodeQuery.Type = KMTQAITYPE_NODEPERFDATA;
    nodeQuery.pPrivateDriverData = &nodeData;
    nodeQuery.PrivateDriverDataSize = sizeof(nodeData);

    status = D3DKMTQueryAdapterInfo(&nodeQuery);
    if (status == 0) {
        m_telemetry.coreClockMhz = static_cast<float>(nodeData.Frequency) / 1000000.0f;
    }
}

void ComputeBenchmarker::QueryVramUsage() {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4)))) return;

    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
    if (FAILED(factory4->EnumAdapterByLuid(m_adapterLuid, IID_PPV_ARGS(&adapter3)))) return;

    DXGI_QUERY_VIDEO_MEMORY_INFO memInfo = {};
    if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
        m_telemetry.vramUsedMB = memInfo.CurrentUsage / (1024 * 1024);
        if (m_telemetry.vramTotalMB > 0) {
            m_telemetry.vramUsagePercent = (static_cast<float>(m_telemetry.vramUsedMB) / 
                                            static_cast<float>(m_telemetry.vramTotalMB)) * 100.0f;
        }
    }
}

void ComputeBenchmarker::UpdateTelemetry() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTelemetryUpdate).count();
    
    // Query every 500ms to avoid overhead
    if (elapsed < 500) return;
    m_lastTelemetryUpdate = now;

    QueryGpuTemperature();
    QueryVramUsage();
}

bool ComputeBenchmarker::CompileAndSwapShader(const std::wstring& hlslPath) {
    ComPtr<ID3DBlob> computeBlob = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

    HRESULT hr = D3DCompileFromFile(
        hlslPath.c_str(), nullptr, nullptr, 
        "main", "cs_5_0", 
        flags, 0, &computeBlob, &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << "\n[HLSL HOT-RELOAD ERROR]\n" 
                      << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return false;
    }

    ComPtr<ID3D11ComputeShader> newShader;
    hr = m_device->CreateComputeShader(computeBlob->GetBufferPointer(), computeBlob->GetBufferSize(), nullptr, &newShader);
    
    if (SUCCEEDED(hr)) {
        std::lock_guard<std::mutex> lock(m_shaderMutex);
        m_computeShader = newShader;
        std::cout << "[ComputeBenchmarker] HLSL File updated -> Compiled & Hot-Swapped Successfully!" << std::endl;
        return true;
    }
    return false;
}

void ComputeBenchmarker::CheckForShaderUpdates() {
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(m_shaderSourcePath.c_str(), GetFileExInfoStandard, &fileInfo)) {
        if (CompareFileTime(&fileInfo.ftLastWriteTime, &m_lastShaderWriteTime) > 0) {
            Sleep(100); 
            m_lastShaderWriteTime = fileInfo.ftLastWriteTime;
            CompileAndSwapShader(m_shaderSourcePath);
        }
    }
}

bool ComputeBenchmarker::LoadShader(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[ComputeBenchmarker] Shader CSO not found. Path: " << std::string(path.begin(), path.end()) << std::endl;
        return false;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    HRESULT hr = m_device->CreateComputeShader(buffer.data(), buffer.size(), nullptr, &m_computeShader);
    
    if (FAILED(hr)) {
        std::cerr << "[ComputeBenchmarker] CreateComputeShader failed!" << std::endl;
        return false;
    }
    return true;
}

void ComputeBenchmarker::DispatchWorkload() {
    std::lock_guard<std::mutex> lock(m_shaderMutex);
    
    if (!m_context || !m_computeShader) return;

    m_context->CSSetShader(m_computeShader.Get(), nullptr, 0);
    ID3D11UnorderedAccessView* passUav[] = { m_uav.Get() };
    m_context->CSSetUnorderedAccessViews(0, 1, passUav, nullptr);

    // 1024 Thread Group * 256 numthreads = 262,144 concurrent threads
    m_context->Dispatch(1024, 1, 1);
    m_context->Flush();
}

} // namespace graphics
