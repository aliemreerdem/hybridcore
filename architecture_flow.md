# HybridCore: Architecture and Flow Diagram (Work Stealing)

The **Mermaid.js** based diagram below illustrates the internal architecture at the C++ level, the Directed Acyclic Graph (DAG) dependency management, and how the dedicated hardware worker pools asynchronously "steal" jobs from the centralized thread-safe queues lock-free.

```mermaid
graph TD
    classDef engine fill:#2d3436,stroke:#dfe6e9,stroke-width:2px,color:#fff;
    classDef router fill:#0984e3,stroke:#74b9ff,stroke-width:2px,color:#fff;
    classDef queue fill:#e17055,stroke:#fab1a0,stroke-width:2px,color:#fff;
    classDef worker fill:#00b894,stroke:#55efc4,stroke-width:2px,color:#fff;
    classDef hardware fill:#6c5ce7,stroke:#a29bfe,stroke-width:3px,color:#fff;

    Engine["Engine Main Loop (16ms Tick)"]:::engine -.->|"Generates Batch"| DAG["DAG: Job Dependencies (Pre-Process -> AI -> Post-Process)"]:::engine
    DAG -.->|"Submit"| JobRouter["JobRouter (Dependency Resolver)"]:::router

    JobRouter -->|"Job Unblocked & Type: HEAVY_COMPUTE"| GPUQ[("Global GPU Pool (ThreadSafeJobQueue)")]:::queue
    JobRouter -->|"Job Unblocked & Type: AI_INFERENCE"| NPUQ[("Global NPU Pool (ThreadSafeJobQueue)")]:::queue

    subgraph CompetingConsumers ["Competing Consumers (Work Stealing)"]
        W1["GPU Worker 1 (eGPU)"]:::worker
        W2["GPU Worker 2 (iGPU)"]:::worker
        W3["NPU Worker (Ryzen NPU)"]:::worker
    end
    
    GPUQ -.->|"Async Pop (Steal Job)"| W1
    GPUQ -.->|"Async Pop (Steal Job)"| W2
    NPUQ -.->|"Async Pop (Steal Job)"| W3

    W1 == "D3D11 Dispatch" ==> HW1{{"AMD Radeon RX 9070 (Discrete GPU 100%)"}}:::hardware
    W2 == "D3D11 Dispatch" ==> HW2{{"AMD Radeon 890M (Integrated GPU 100%)"}}:::hardware
    W3 == "WinML Evaluate" ==> HW3{{"Ryzen AI NPU (MCDM 100%)"}}:::hardware

    HW1 -.->|"OnJobCompleted"| JobRouter
    HW2 -.->|"OnJobCompleted"| JobRouter
    HW3 -.->|"OnJobCompleted"| JobRouter
```

## Decision Mechanisms and Workflow
1. **Engine Tick:** `Engine::Update()` continuously monitors the system. Whenever the system has capacity to breathe, it packages a new block of work (Batch) consisting of a chain of hardware dependencies (J1, J2, J3) and forwards it to the `JobRouter`.
2. **DAG Resolution (Dependency Chain):** The `JobRouter` evaluates the `dependencies` list of incoming jobs. For instance, `J2 (AI)` is never scheduled until `J1 (eGPU)` completes. Once `J1` finishes, `J2` is unblocked.
3. **Queue Routing (Integrated Pools):** Unblocked jobs are dispatched into one of two massive, lockable pools (`ThreadSafeJobQueue`) based on their computation type: one for **GPUs** (`HEAVY_COMPUTE`) and another for **NPUs** (`AI_INFERENCE`).
4. **Work Stealing Architecture:** At this stage, the system abandons "fairness" and becomes completely aggressive! As soon as a GPU Worker finishes its active task, it instantaneously queries the `Global GPU Pool` and steals a new job with extreme speed (breaking the async lock via `cv.wait`). Because the RX 9070 is significantly faster in this race compared to the integrated 890M, it organically shoulders almost the entirety of the workload, pinning constantly at 100% utilization.
5. **Feedback Loop:** Upon hardware execution completion, an `OnJobCompleted` event is fired back to the Router. The `JobRouter` centralizes this event, and if any blocked job depends on it (e.g., J3 waking up after J2 concludes), the Router releases it into the Global pool, perpetuating the system iteration indefinitely.
