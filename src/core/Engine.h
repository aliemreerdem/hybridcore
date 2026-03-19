#pragma once
#include "Window.h"
#include "JobRouter.h"
#include "../graphics/ComputeBenchmarker.h"
#include "../ai/NpuEngine.h"

#include <d3d11.h>
#include <dxgi.h>

#include <memory>
#include <atomic>
#include <chrono>
#include <vector>

namespace core {

class Engine {
public:
    Engine();
    ~Engine();

    void Initialize();
    void Run();
    void Shutdown();

private:
    void Update();
    void Render();
    
    // Uygulama Ici Menu Olaylarini (Event) karsilayan dinleyici metodumuz:
    void HandleMenuEvent(int menuId);

    std::unique_ptr<Window> m_window;
    std::unique_ptr<JobRouter> m_jobRouter;
    std::vector<std::unique_ptr<graphics::ComputeBenchmarker>> m_gpuBenchmarkers;
    std::unique_ptr<ai::NpuEngine> m_npuEngine;
    bool m_isRunning;
    std::atomic<bool> m_isSimulating;
    uint64_t m_batchCounter;
    
    std::chrono::steady_clock::time_point m_lastLogTime;
    uint64_t m_lastEGpuCount;
    uint64_t m_lastNpuCount;

    // --- Overlay Render Pipeline ---
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_mainRenderTargetView;

    void CreateRenderTarget();
    void CleanupRenderTarget();
};

} // namespace core
