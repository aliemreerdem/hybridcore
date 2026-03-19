#include "Engine.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_impl_win32.h"
#include "../vendor/imgui/imgui_impl_dx11.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>

namespace core {

Engine::Engine() : m_isRunning(false), m_isSimulating(false), m_batchCounter(0), m_lastEGpuCount(0), m_lastNpuCount(0) {}
Engine::~Engine() {}

void Engine::CreateRenderTarget() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_d3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_mainRenderTargetView);
}

void Engine::CleanupRenderTarget() {
    if (m_mainRenderTargetView) { m_mainRenderTargetView.Reset(); }
}

void Engine::Initialize() {
    std::cout << "[Engine] Initializing subsystems..." << std::endl;
    m_window = std::make_unique<Window>(1280, 720, L"HybridCore AI Analysis Tool");
    
    // Bind window menu callback to Engine
    m_window->SetMenuCallback([this](int menuId) {
        this->HandleMenuEvent(menuId);
    });

    // --- Bootstrapping OS Default Device & SwapChain for the UI Overlay ---
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_window->GetHWND();
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    
    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_swapChain, &m_d3dDevice, &featureLevel, &m_d3dContext);

    if (FAILED(res)) {
        std::cerr << "[Engine] Internal SwapChain deployment collapsed. ImGui binding will abort." << std::endl;
    } else {
        CreateRenderTarget();

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(m_window->GetHWND());
        ImGui_ImplDX11_Init(m_d3dDevice.Get(), m_d3dContext.Get());
        std::cout << "[Engine] Dear ImGui Overlay Mounted over Native OS context." << std::endl;
    }

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
        // [Hot-Reloading] Poll shader changes per frame
        for (auto& gpu : m_gpuBenchmarkers) {
            gpu->CheckForShaderUpdates();
        }

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
    }
}

void Engine::Render() {
    if (!m_d3dContext) return;

    // Push ImGui Frame Contexts
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Render Dynamic Performance Dashboard
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("HybridCore Load Balancer & Telemetry", nullptr, ImGuiWindowFlags_NoCollapse);
    
    if (m_jobRouter) {
        ImGui::Text("Global Active Tasks : %zu", m_jobRouter->GetTotalJobCount());
        ImGui::Separator();
        ImGui::Text("RX 9070 (eGPU) Tasks Completed : %llu", m_jobRouter->GeteGpuCompleted());
        ImGui::Text("Ryzen AI (NPU) Tasks Completed: %llu", m_jobRouter->GetNpuCompleted());
    } else {
        ImGui::Text("Job Router mapping disabled.");
    }
    
    if (m_isSimulating) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "SIMULATION IN PROGRESS (Maximum Hardware Bind)");
    } else {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "ENGINE IDLE (Awaiting Win32 Events)");
    }

    ImGui::End();

    ImGui::Render();

    const float clear_color[4] = { 0.08f, 0.1f, 0.12f, 1.0f }; // Sleek dark slate
    m_d3dContext->OMSetRenderTargets(1, m_mainRenderTargetView.GetAddressOf(), nullptr);
    m_d3dContext->ClearRenderTargetView(m_mainRenderTargetView.Get(), clear_color);
    
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swapChain->Present(1, 0); // V-Sync enabled output
}

void Engine::Shutdown() {
    std::cout << "[Engine] Shutdown initiated, cleaning up engine memory." << std::endl;
    
    // Shutting down external ImGui hooks
    if (m_d3dDevice) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        CleanupRenderTarget();
    }

    m_window.reset();
}

} // namespace core
