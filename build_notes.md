# GPU Stress Tester - Technical Build & Architecture Notes

This document serves as a historical record of the technical challenges, rendering heuristics, and architectural decisions made during the development of the C++ DirectX 11 GPU Stress Tester.

## 1. OOP Architecture Transformation (C++ Game Engine Pattern)
The project initially began as a monolithic `main.cpp` procedural script containing all Win32 Window creation, ID3D11 device contexts, and shader logic. To ensure stability and adhere to professional game engine standards, it was shattered into encapsulated components:
*   **`core/Engine`**: The central orchestrator. Runs the Carmack-style fixed tick message pump and manages the testing lifecycle.
*   **`core/Window`**: Encapsulates all Win32 API calls (`CreateWindowEx`, `PeekMessage`), cleanly separating OS message handling from graphics.
*   **`graphics/Renderer`**: Manages the DXGI SwapChain, Direct3D 11 Device, render targets, depth stencils, and dummy shaders. Also handles hardware adapter (GPU) enumeration.
*   **`graphics/ComputeBenchmarker`**: Exclusively dedicated to loading HLSL Compiled Shader Objects (`.cso`), managing UAV/SRV buffers, and dispatching thread groups.

## 2. Overcoming AMD Adrenalin & Desktop Window Manager (DWM) Heuristics
A primary challenge was forcing graphics driver overlays (like AMD Adrenalin) to recognize a purely "Compute-only" command prompt program as a "3D Game" in Windowed mode.

**The Heuristic Bypass Recipe:**
1.  **3D Pipeline Faking**: We attached a `D3D11_DEPTH_STENCIL_DESC` and bound an empty Render Target View.
2.  **Input Assembler Overload**: Drivers ignore applications with zero vertex counts. We created `vs_dummy.cso` and `ps_dummy.cso` and executed a massive `Draw(300000, 0)` command per frame to fake a high-end 3D geometry engine, deceiving the Input Assembler heuristic.
3.  **Active Uniform Buffers**: Constant Buffers (`D3D11_USAGE_DYNAMIC`) are memory-mapped/unmapped every frame to mimic a real game engine updating View/Projection matrices.
4.  **True Borderless Fullscreen**: DWM forces high-DPI scaling down to Standard Windowed mode if `SM_CXSCREEN` resolutions don't match. We programmatically scale the SwapChain bounds to native monitor resolution to ensure Independent Flip mode presentation.

## 3. Dispatch Quantization & The GPU Starvation Fix
To trigger the overlay, the application must push **>60 Frames Per Second**. However, pushing 16 million element matrix math workloads natively took ~250ms per frame (4 FPS).

*   **The Slicing Fix:** We instituted "Dispatch Quantization". A 16M element workload is sliced into 16 smaller dispatches of 1M elements each. The engine calls `Present()` after each slice, rapidly inflating the FPS to ~70+ while maintaining 100% compute load.

*   **The OS Timer Resolution Bug (Starvation Case):**
    Initially, the quantization loop included a `Sleep(1)` call to pace the CPU thread and prevent Windows from flagging the app as a "Synthetic Spinlock" (Not Responding).
    However, the Win32 CPU timer resolution minimum is generally **15.6ms**.
    High-end GPUs (like the RX 9070) computed the 1M element slice in **under 1ms**. The thread then slept for 15.6ms. The GPU finished its queue instantly and sat idle, resulting in **27% GPU Utilization** ("Starvation").

*   **The Solution:** The `Sleep(1)` function was dragged *outside* the 16x slice loop. The C++ motor rapidly blasts 16 continuous `Present()` packets directly into the DXGI driver queue, maintaining a completely unbroken hardware pipeline, and only yields the CPU thread once the full logical frame completes. This restored true **100% GPU Saturation**.

## 4. Standalone Deployment & Portability
*   **Offline Shaders:** Direct3D runtime compilation (`<d3dcompiler.h>`) requires the end-user to have specific Windows SDK DLLs installed. We shifted to offline compilation using `fxc.exe` within `build.bat`, generating headless `.cso` (Compiled Shader Object) payloads.
*   **MSVC Runtime Baking:** `build.bat` executes `cl.exe` using the `/MT` (Multithreaded Static) flag. This physically bundles the `vcruntime140.lib` C++ standard library dependencies straight into `Game.exe`, allowing the Stress Tester to run natively on freshly installed Windows PCs without error.
