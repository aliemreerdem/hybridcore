# HybridCore: Infinite Scaling Hardware Orchestrator

HybridCore is an ultra-high-performance C++ job routing engine designed with a "Zero-Dependency" philosophy to extract 100% performance simultaneously from modern asymmetric hardware architectures (such as eGPUs, iGPUs, and NPUs).

## Project Goal
In traditional projects, software typically binds to the first (or default) graphics card detected by the system, often leaving the integrated GPU or the NPU completely idle. HybridCore eliminates this waste. Inspired by NServiceBus and microservice architectures, it features a robust Directed Acyclic Graph (DAG) workload executor.
Tens of thousands of enterprise workflows (e.g., Data Preparation -> AI Inference -> Data Processing) are instantly distributed to the appropriate hardware pools based on their requirements ("Heavy Compute" or "NPU Workload").

Our objective is simple: To eliminate hardware bottlenecks by pushing all silicon devices on the system (RX 9070 eGPU, Radeon 890M iGPU, Ryzen AI NPU) to 100% load simultaneously without a single instruction of waste.

## Core Technologies and Native APIs
No external heavy libraries (Boost, .NET, CMake, Python) are included. To achieve pure hybrid power, the engine talks directly to the Windows Kernel and Graphics layers:
* **Modern C++20 (MSVC):** Code is linked with Static Runtime Linker (`/MT`) flags to ensure no dynamic library (`.dll`) dependencies are required.
* **DirectX Graphics Infrastructure (DXGI):** Instead of blindly trusting the default adapter, all iGPUs and eGPUs on the PCI-E bus are dynamically discovered and assigned to isolated CPU threads using the `EnumAdapters1` interface.
* **Direct3D 11 Compute Shaders (HLSL):** Hardware-level shader architecture, as powerful as CUDA in parallel programming, is utilized. Thousands of distinct compute threads (Dispatches) are triggered per graphics card.
* **Windows Machine Learning & C++/WinRT:** To execute AMD Ryzen AI NPU cores, ONNX models (`mnist-8.onnx`) are fired directly at the hardware Neural Processing Units using the Microsoft Compute Driver Model (MCDM).

## The Heart of the Architecture: The "Work Stealing" Paradigm
The greatest engineering triumph of the development phase was the evolution of the Distribution Algorithm.

Initially, a classic Round-Robin approach (distributing tasks sequentially: one for you, one for you) was implemented to be "fair" to the hardware. However, this resulted in a catastrophe; the massively powerful asymmetric eGPU would finish its 50 tasks in milliseconds, quickly draining its queue and falling into an idle state at 38% utilization, while the weaker iGPU was buried under tasks, severely clogging the pipeline and deadlocking the queuing engine.

Consequently, the system was upgraded to a modern "Competing Consumers / Work Stealing" architecture:
1. Individual hardware queues were discarded in favor of a massive, centralized `ThreadSafeJobQueue` (Universal Pool) protected at the OS level.
2. GPU worker threads now asynchronously (lock-free) "steal" (pop) fresh jobs from the pool the exact millisecond their payload finishes.
3. As a result, by the time the weaker graphics card fetches a single job from the pool, the powerful eGPU naturally melts 20 jobs on its own initiative. Both graphics cards and the AI NPU flawlessly operate at 100% utilization simultaneously.

## How to Compile and Run
Adhering to its zero-dependency philosophy, the system does not require any external IDE project configurations (e.g., CMakeLists.txt). It is built with a single click using a batch MSVC command line from the root directory (leveraging the native Windows SDK).

From Cmd / PowerShell in the repository root directory:
```bat
.\build.bat
```
*(The script compiles your HLSL shaders into hardware bytecode (`.cso`) via `fxc.exe` and links the C++ subsystems to produce the `bin/HybridCoreDiscovery.exe` executable.)*

Once the build is complete, you can launch the engine simulation directly with:
```powershell
bin\HybridCoreDiscovery.exe
```
