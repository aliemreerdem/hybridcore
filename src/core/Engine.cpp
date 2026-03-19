#include "Engine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace core {

Engine::Engine() : m_isRunning(false), m_isSimulating(false), m_batchCounter(0), m_lastEGpuCount(0), m_lastNpuCount(0) {}
Engine::~Engine() {}

void Engine::Initialize() {
    std::cout << "[Engine] Initializing subsystems..." << std::endl;
    m_window = std::make_unique<Window>(1280, 720, L"HybridCore AI Analysis Tool");
    
    // Bind window menu callback to Engine
    m_window->SetMenuCallback([this](int menuId) {
        this->HandleMenuEvent(menuId);
    });

    m_jobRouter = std::make_unique<JobRouter>();
    
    // 1. Sistemdeki tüm GPU'ları (eGPU, iGPU vs.) DXGI ile tespit et ve her birine özel bir donanım işçisi kur
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    UINT i = 0;
    while (factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        // Sadece donanımsal GPU'ları filtrele (Software Render'ları atla)
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            auto benchmarker = std::make_unique<graphics::ComputeBenchmarker>();
            // D3D11 cihazini bu spesifik adapter'e bagla
            if (benchmarker->Initialize(i)) {
                char nameStr[128];
                size_t c = 0;
                wcstombs_s(&c, nameStr, sizeof(nameStr), desc.Description, _TRUNCATE);
                std::string workerName = std::string(nameStr) + " Worker";

                auto* bPtr = benchmarker.get();
                m_jobRouter->RegisterGpuWorker(workerName, [bPtr](const Job&) { 
                    bPtr->DispatchWorkload(); 
                });
                
                m_gpuBenchmarkers.push_back(std::move(benchmarker));
            }
        }
        adapter.Reset();
        i++;
    }

    if (m_gpuBenchmarkers.empty()) {
        std::cerr << "[Engine] DIKKAT: GPU donanimina erisilemedi. Yük yönlendirilemeyecek." << std::endl;
    }

    // Windows Machine Learning NPU (Ryzen AI) arayüzünü başlat
    m_npuEngine = std::make_unique<ai::NpuEngine>();
    if (m_npuEngine->Initialize(L"bin\\models\\mnist-8.onnx")) {
        auto* nPtr = m_npuEngine.get();
        m_jobRouter->RegisterNpuWorker("Ryzen AI NPU Worker", [nPtr](const Job&) { 
            nPtr->EvaluateDummyPayload(); 
        });
    } else {
        std::cerr << "[Engine] DIKKAT: NPU ONNX modellemesi basarisiz." << std::endl;
    }

    m_isRunning = true;
    std::cout << "[Engine] Window system, Menu, and hardware infrastructures ready." << std::endl;
}

void Engine::HandleMenuEvent(int menuId) {
    switch(menuId) {
        case ID_MENU_EXIT:
            std::cout << "[Engine-Menu] -> Exit selected. Shutting down gracefully..." << std::endl;
            m_isRunning = false; // Terminate loop
            break;
            
        case ID_MENU_HARDWARE_TEST:
            std::cout << "[Engine-Menu] -> START CONTINUOUS SIMULATION: Maxing out workloads..." << std::endl;
            m_lastLogTime = std::chrono::steady_clock::now();
            m_lastEGpuCount = m_jobRouter ? m_jobRouter->GeteGpuCompleted() : 0;
            m_lastNpuCount = m_jobRouter ? m_jobRouter->GetNpuCompleted() : 0;
            m_isSimulating = true;
            break;
            
        case ID_MENU_STOP_TEST:
            std::cout << "[Engine-Menu] -> STOP SIMULATION: Clearing queues and halting injection." << std::endl;
            m_isSimulating = false;
            if (m_jobRouter) {
                m_jobRouter->ClearJobs(); // Immediately scrub pending/queued workloads
            }
            break;
            
        default:
            break;
    }
}

void Engine::Run() {
    std::cout << "[Engine] Entering main engine loop (Fixed-Tick)..." << std::endl;
    
    while (m_isRunning) {
        if (!m_window->ProcessMessages()) {
            m_isRunning = false;
            break;
        }

        Update();
        Render();

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); 
    }
    std::cout << "[Engine] Main loop terminated." << std::endl;
}

void Engine::Update() {
    if (m_jobRouter) {
        // If simulation is running and we have less than 12 active jobs system-wide (approx 4 parallel DAG batches), inject more work
        if (m_isSimulating && m_jobRouter->GetTotalJobCount() < 12) {
            uint64_t bId = m_batchCounter++;
            std::string j1 = "J1_" + std::to_string(bId);
            std::string j2 = "J2_" + std::to_string(bId);
            std::string j3 = "J3_" + std::to_string(bId);

            // JobPayload logic is managed locally per worker, removing lambda bindings
            m_jobRouter->SubmitJob({j1, "Pre-process (Batch " + std::to_string(bId) + ")", JobType::HEAVY_COMPUTE, {}});
            m_jobRouter->SubmitJob({j2, "AI Inference (Batch " + std::to_string(bId) + ")", JobType::AI_INFERENCE, {j1}});
            m_jobRouter->SubmitJob({j3, "Post-process (Batch " + std::to_string(bId) + ")", JobType::HEAVY_COMPUTE, {j2}});
        }
        
        m_jobRouter->Update(); // Evaluate DAG and dispatch jobs to workers
        
        if (m_isSimulating) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastLogTime).count() >= 1000) {
                uint64_t curEGpu = m_jobRouter->GeteGpuCompleted();
                uint64_t curNpu = m_jobRouter->GetNpuCompleted();
                std::cout << "[System Metrics] eGPU Throughput: " << (curEGpu - m_lastEGpuCount) 
                          << " jobs/s | NPU Throughput: " << (curNpu - m_lastNpuCount) 
                          << " jobs/s | Processing Queue Load: " << m_jobRouter->GetTotalJobCount() << std::endl;
                
                m_lastEGpuCount = curEGpu;
                m_lastNpuCount = curNpu;
                m_lastLogTime = now;
            }
        }
    }
}

void Engine::Render() {}

void Engine::Shutdown() {
    std::cout << "[Engine] Shutdown initiated, cleaning up engine memory." << std::endl;
    m_window.reset();
}

} // namespace core
