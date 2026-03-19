#include "ComputeBenchmarker.h"
#include <iostream>
#include <fstream>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace graphics {

ComputeBenchmarker::ComputeBenchmarker() {}

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

    if (!LoadShader(L"bin\\shaders\\compute.cso")) {
        return false;
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

bool ComputeBenchmarker::LoadShader(const std::wstring& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[ComputeBenchmarker] Shader CSO bulunamadi. Yol: bin\\shaders\\compute.cso" << std::endl;
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
