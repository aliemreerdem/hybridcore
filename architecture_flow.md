# HybridCore: Mimari ve Akış Diyagramı (Work Stealing)

Aşağıdaki **Mermaid.js** tabanlı diyagram, motorun C++ seviyesindeki çalışma mantığını, bağımlılık (DAG) yönetim sistemini ve özel donanım işçilerinin (Worker Pool) merkezi havuzdan nasıl kilitlenmeden (lock-free) asenkron iş çaldıklarını göstermektedir.

```mermaid
graph TD
    classDef engine fill:#2d3436,stroke:#dfe6e9,stroke-width:2px,color:#fff;
    classDef router fill:#0984e3,stroke:#74b9ff,stroke-width:2px,color:#fff;
    classDef queue fill:#e17055,stroke:#fab1a0,stroke-width:2px,color:#fff;
    classDef worker fill:#00b894,stroke:#55efc4,stroke-width:2px,color:#fff;
    classDef hardware fill:#6c5ce7,stroke:#a29bfe,stroke-width:3px,color:#fff;

    Engine["Engine Main Loop (16ms Tick)"]:::engine -.->|"Generates Batch"| DAG["DAG: İş Bağımlılıkları (Pre-Process -> AI -> Post-Process)"]:::engine
    DAG -.->|"Submit"| JobRouter["JobRouter (Bağımlılık Çözücü)"]:::router

    JobRouter -->|"Job Unblocked & Type: HEAVY_COMPUTE"| GPUQ[("Global GPU Pool (ThreadSafeJobQueue)")]:::queue
    JobRouter -->|"Job Unblocked & Type: AI_INFERENCE"| NPUQ[("Global NPU Pool (ThreadSafeJobQueue)")]:::queue

    subgraph CompetingConsumers ["Competing Consumers (Work Stealing)"]
        W1["GPU Worker 1 (eGPU)"]:::worker
        W2["GPU Worker 2 (iGPU)"]:::worker
        W3["NPU Worker (Ryzen NPU)"]:::worker
    end
    
    GPUQ -.->|"Async Pop (İş Çal)"| W1
    GPUQ -.->|"Async Pop (İş Çal)"| W2
    NPUQ -.->|"Async Pop (İş Çal)"| W3

    W1 == "D3D11 Dispatch" ==> HW1{{"AMD Radeon RX 9070 (Discrete GPU %100)"}}:::hardware
    W2 == "D3D11 Dispatch" ==> HW2{{"AMD Radeon 890M (Integrated GPU %100)"}}:::hardware
    W3 == "WinML Evaluate" ==> HW3{{"Ryzen AI NPU (MCDM %100)"}}:::hardware

    HW1 -.->|"OnJobCompleted"| JobRouter
    HW2 -.->|"OnJobCompleted"| JobRouter
    HW3 -.->|"OnJobCompleted"| JobRouter
```

## Karar Mekanizmaları ve İş Akışı
1. **Engine Tick (Motor Döngüsü):** `Engine::Update()` sistemi sürekli izler. Sistemin nefes alabileceği yer varsa, donanımlara (J1, J2, J3) zincirinden oluşan yeni bir iş bloğu (Batch) paketler ve *JobRouter*'a iletir.
2. **DAG Çözümü (Bağımlılık Zinciri):** `JobRouter` gelen işlerin `dependencies` (bağımlılık) listesini kontrol eder. Örneğin `J2 (AI)`, `J1 (eGPU)` bitmeden devreye alınmaz. `J1` bitince `J2` tetiklenir.
3. **Kuyruk Yönlendirmesi (Entegre Havuzlar):** Bağımlılığı çözülen işler, türlerine göre iki devasa havuza kilitlenebilir kuyruk (ThreadSafeJobQueue) kullanılarak bırakılır: biri **GPU**'lar için (HEAVY_COMPUTE), diğeri **NPU**'lar için (AI_INFERENCE).
4. **İş Çalma (Work Stealing Mimarisi):** Burada sistem adaleti bir kenara bırakır ve acımasız davranır! GPU Worker'lar her kendi işleri biter bitmez `Global GPU Pool`'dan anlık olarak (`cv.wait` üzerinden async lock kırarak) radikal bir hızda yeni bir İş Çalarlar. RX 9070 bu yarışta 890M'e göre aşırı süratli olduğu için iş yükünün tamamına yakınını organik olarak sırtlar ve %100 orana vurur.
5. **Geri Bildirim Döngüsü:** Donanımların işlemleri bittiğinde, `OnJobCompleted` olayı (Event) Router'a fırlatılır. `JobRouter` bunu merkeze alır ve bağlı olduğu bir iş varsa (Örneğin J3, J2 bitince uyanacaktır) onu Global havuza serbest bırakır ve sistem döngüsü devam eder.
