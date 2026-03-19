# HybridCore: Architecture and Flow Diagram (Industrial Grade)

The **Mermaid.js** based diagram below illustrates the internal architecture at the C++ level, highlighting the Directed Acyclic Graph (DAG) dependency management, the Zero-Allocation Arena, and how dedicated hardware worker pools asynchronously "steal" jobs lock-free while communicating with a Live ImGui Telemetry layer.

```mermaid
graph TD
    classDef engine fill:#2d3436,stroke:#dfe6e9,stroke-width:2px,color:#fff;
    classDef router fill:#0984e3,stroke:#74b9ff,stroke-width:2px,color:#fff;
    classDef queue fill:#e17055,stroke:#fab1a0,stroke-width:2px,color:#fff;
    classDef worker fill:#00b894,stroke:#55efc4,stroke-width:2px,color:#fff;
    classDef hardware fill:#6c5ce7,stroke:#a29bfe,stroke-width:3px,color:#fff;
    classDef external fill:#d63031,stroke:#ff7675,stroke-width:2px,color:#fff;
    classDef ui fill:#fdcb6e,stroke:#ffeaa7,stroke-width:2px,color:#2d3436;

    Engine["Engine Main Loop (Win32/DX11)"]:::engine
    HLSL["compute.hlsl (Live Hot-Reloading)"]:::external
    UI["Dear ImGui Telemetry Dashboard"]:::ui
    DAG["DAG: Job Dependencies"]:::engine
    MemoryArena["Global JobPool (Zero-Allocation Arena)"]:::engine
    JobRouter["JobRouter (Dependency Resolver)"]:::router
    ErrorLog["error.log (Global SEH)"]:::external

    Engine -->|"Poll Filesystem"| HLSL
    Engine -->|"Renders UI"| UI
    Engine -->|"Generates Batch"| DAG
    
    DAG -->|"Checkout Pointer"| MemoryArena
    MemoryArena -->|"Submit Job"| JobRouter

    GPUQ[("Global GPU Pool (ThreadSafeJobQueue)")]:::queue
    NPUQ[("Global NPU Pool (ThreadSafeJobQueue)")]:::queue

    JobRouter -->|"Type: HEAVY_COMPUTE"| GPUQ
    JobRouter -->|"Type: AI_INFERENCE"| NPUQ
    JobRouter -->|"Any Crashing Error"| ErrorLog

    subgraph CompetingConsumers ["Competing Consumers (Work Stealing)"]
        W1["GPU Worker 1 (eGPU)"]:::worker
        W2["GPU Worker 2 (iGPU)"]:::worker
        W3["NPU Worker (Ryzen NPU)"]:::worker
    end
    
    GPUQ -->|"Async Pop"| W1
    GPUQ -->|"Async Pop"| W2
    NPUQ -->|"Async Pop"| W3

    HW1{{"AMD Radeon RX 9070 (Discrete GPU 100%)"}}:::hardware
    HW2{{"AMD Radeon 890M (Integrated GPU 100%)"}}:::hardware
    HW3{{"Ryzen AI NPU (MCDM 100%)"}}:::hardware

    W1 -->|"D3D11 Dispatch"| HW1
    W2 -->|"D3D11 Dispatch"| HW2
    W3 -->|"WinML Evaluate"| HW3
    
    HLSL -.->|"D3DCompileFromFile"| HW1

    HW1 -.->|"Recycle Pointer"| JobRouter
    HW2 -.->|"Recycle Pointer"| JobRouter
    HW3 -.->|"Recycle Pointer"| JobRouter
```

## Decision Mechanisms and Workflow
1. **Engine Tick:** `Engine::Update()` continuously monitors the system. Whenever the system has capacity to breathe, it allocates raw pointers from the static `JobPool` array guaranteeing absolutely zero heap fragmentation (GC pauses) during intensive workloads.
2. **DAG Resolution (Dependency Chain):** The `JobRouter` evaluates the `dependencies` list of incoming jobs. For instance, `J2 (AI)` is never scheduled until `J1 (eGPU)` completes. Once `J1` finishes, `J2` is unblocked.
3. **Queue Routing (Integrated Pools):** Unblocked jobs are dispatched into one of two massive, lockable pools (`ThreadSafeJobQueue`) based on their computation type: one for **GPUs** (`HEAVY_COMPUTE`) and another for **NPUs** (`AI_INFERENCE`).
4. **Work Stealing Architecture:** At this stage, the system abandons "fairness" and becomes completely aggressive! As soon as a GPU Worker finishes its active task, it instantaneously queries the `Global GPU Pool` and steals a new job with extreme speed.
5. **Dynamic Shader Reloading:** During hardware iteration, `compute.hlsl` modification times are actively measured by the system. Any IDE save physically invokes a background D3D Compiler replacing the GPU kernel entirely without rebooting the C++ environment.
6. **Hardware Feedback Loop & Telemetry:** Upon hardware execution completion, an `OnJobCompleted` event is fired back to the Router. Simultaneously, the `Dear ImGui` framework processes the Router's global statistics rendering identical real-time distributions directly above the original desktop window. If any catastrophic failures occur, `SetUnhandledExceptionFilter` catches the unmanaged faults logging immutable details to `error.log`.
