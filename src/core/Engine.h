#pragma once
#include "Window.h"
#include "JobRouter.h"
#include "../graphics/ComputeBenchmarker.h"
#include "../ai/NpuEngine.h"
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
};

} // namespace core
