#include "ComputeBenchmarker.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace graphics {

ComputeBenchmarker::ComputeBenchmarker() {
    m_lastShaderWriteTime.dwLowDateTime = 0;
    m_lastShaderWriteTime.dwHighDateTime = 0;
    m_shaderSourcePath = L"src\\shaders\\compute.hlsl";
}

ComputeBenchmarker::~ComputeBenchmarker() {}

bool ComputeBenchmarker::Initialize(UINT adapterIndex) {
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    // DXGI uzerinden spesifik adapter (Ornegin: 0 -> 9070, 1 -> 890M) alinir.
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(factory->EnumAdapters1(adapterIndex, &adapter))) {
        std::cerr << "[ComputeBenchmarker] Adapter " << adapterIndex << " could not be obtained!" << std::endl;
        return false;
    }

    // Hedef adapter uzerinde D3D11 cihazi yarat. Driver type UNKNOWN olmak zorundadir cunku adapter verdik.
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

    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (GetFileAttributesExW(m_shaderSourcePath.c_str(), GetFileExInfoStandard, &fileInfo)) {
        m_lastShaderWriteTime = fileInfo.ftLastWriteTime;
        CompileAndSwapShader(m_shaderSourcePath);
    } else {
        std::cerr << "[ComputeBenchmarker] Warning: compute.hlsl source file missing! Falling back to cached payload." << std::endl;
        LoadShader(L"bin\\shaders\\compute.cso");
    }

    // Islenmis verileri depolamak uzere Unordered Access View (UAV) buffer yaratimi
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
            // Wait slightly for the file lock to be fully released by external editors
            Sleep(100); 
            m_lastShaderWriteTime = fileInfo.ftLastWriteTime;
            CompileAndSwapShader(m_shaderSourcePath);
        }
    }
}

bool ComputeBenchmarker::LoadShader(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[ComputeBenchmarker] Shader CSO bulunamadi. Yol: " << std::string(path.begin(), path.end()) << std::endl;
        return false;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    HRESULT hr = m_device->CreateComputeShader(buffer.data(), buffer.size(), nullptr, &m_computeShader);
    
    if (FAILED(hr)) {
        std::cerr << "[ComputeBenchmarker] CreateComputeShader husrana ugradi!" << std::endl;
        return false;
    }
    return true;
}

void ComputeBenchmarker::DispatchWorkload() {
    std::lock_guard<std::mutex> lock(m_shaderMutex);
    
    if (!m_context || !m_computeShader) return;

    // Buffer'lari ve Shader'i donanim uzerine bagla
    m_context->CSSetShader(m_computeShader.Get(), nullptr, 0);
    ID3D11UnorderedAccessView* passUav[] = { m_uav.Get() };
    m_context->CSSetUnorderedAccessViews(0, 1, passUav, nullptr);

    // Donanima devasa Compute komutu (Dispatch Quantization) basiyoruz.
    // 1024 Thread Group * 256 numthreads = 262,144 concurrent is parcacigi
    m_context->Dispatch(1024, 1, 1);

    // Komutu siradan cikarip ekran kartina aninda isletmesini emrediyoruz
    m_context->Flush();
}

} // namespace graphics
