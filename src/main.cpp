#include "core/Engine.h"
#include <iostream>
#include <vector>
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#pragma comment(lib, "dxgi.lib")
using Microsoft::WRL::ComPtr;

// Donanim sorgulama kodumuzu test/bilgilendirme amaciyla tutuyoruz
void EnumerateAdapters() {
    ComPtr<IDXGIFactory6> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return;

    std::cout << "[DXGI] System Components (Summary):\n";
    ComPtr<IDXGIAdapter1> adapter;
    UINT adapterIndex = 0;
    while (factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
             std::wcout << L"  -> " << desc.Description << L" (VRAM: " << (desc.DedicatedVideoMemory / (1024 * 1024)) << L" MB)\n";
        }
        adapterIndex++;
        adapter.Reset();
    }
    std::cout << "------------------------------------------\n\n";
}

int main() {
    // Konsol karakter destek formati
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "--- HybridCore AI Analysis Tool ---\n" << std::endl;
    
    // Once donanim listesini bas
    EnumerateAdapters(); 

    // Motoru baslat ve pencereli donguye gir
    core::Engine engine;
    engine.Initialize();
    engine.Run();
    engine.Shutdown();

    return 0;
}
