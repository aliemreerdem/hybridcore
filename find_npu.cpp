#include <windows.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <iostream>
#include <string>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

int main() {
    IDXGIFactory6* factory = nullptr;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    if (!factory) {
        std::cout << "Failed to create DXGI factory." << std::endl;
        return 1;
    }

    IDXGIAdapter1* adapter = nullptr;
    UINT i = 0;
    while (factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        std::wcout << L"Adapter " << i << L": " << desc.Description << std::endl;
        adapter->Release();
        i++;
    }

    factory->Release();
    return 0;
}
