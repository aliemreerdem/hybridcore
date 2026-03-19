# Antigravity C++ Geliştirme ve Donanım Optimizasyon Rehberi
## Cihaz: Minisforum X1 Pro (Ryzen AI 9 HX 370) & ASUS RX 9070 (Oculink eGPU)

Bu döküman, Antigravity ekosistemi üzerinde C++ kullanarak yüksek performanslı grafik ve hesaplama birimlerinin (NPU, iGPU, eGPU) yönetimi için mimari bir yol haritası sunar.

---

## 1. Donanım Mimarisi ve İş Yükü Dağıtımı (Heterojen Yapı)

Sisteminizdeki üç farklı işlem birimini Antigravity katmanında şu görevler için optimize etmelisiniz:

| Donanım Birimi | Mimari Katman | C++ Odak Noktası | Görev Tanımı |
| :--- | :--- | :--- | :--- |
| **ASUS RX 9070 (eGPU)** | **Discrete GPU** | Vulkan / DX12 Backend | Ana render motoru, Ray Tracing, Compute Shaders. |
| **Radeon 890M (iGPU)** | **Integrated GPU** | SwapChain / UI Thread | Arayüz çizimi, video işleme, asenkron yükleme ekranları. |
| **Ryzen AI (NPU)** | **XDNA 2 Engine** | DirectML / ONNX Runtime | Gerçek zamanlı AI çıkarımı, kullanıcı davranışı analizi. |

---

## 2. Bellek ve Bant Genişliği Stratejisi (Oculink Optimizasyonu)

Oculink (PCIe Gen4 x4) üzerinden veri aktarımı yaparken C++ tarafında dikkat edilmesi gereken kritik noktalar:

### A. Bellek Yerleşimi (Memory Allocation)
* **Local VRAM Usage:** RX 9070'in 16GB GDDR6 belleği geniştir. Veriyi bir kez eGPU'ya transfer edip, hesaplamaları orada tutun. Sistem RAM'ine sık geri dönüşlerden (Readback) kaçının.
* **Staging Buffers:** Veriyi CPU'dan GPU'ya aktarırken doğrudan transfer yerine asenkron kopyalama komutlarını (`vkCmdCopyBuffer` gibi) tercih edin.

### B. Bant Genişliği Yönetimi
* Oculink hattı (~64 Gbps) PCIe x16 hattına göre dardır. Bu dar boğazı aşmak için **Doku Sıkıştırma (BC7/ASTC)** ve **Mesh Shaders** teknolojilerini Antigravity üzerinde aktif edin.

---

## 3. C++ Seviyesinde Kritik Teknikler

Antigravity üzerinde C++ geliştirirken aşağıdaki modern teknikleri dökümantasyonunuza dahil edin:

1. **AVX-512 Komut Seti:** Ryzen AI 9 HX 370'in Zen 5 mimarisinden yararlanmak için matematiksel kütüphanelerinizi AVX-512 bayraklarıyla derleyin.
2. **Asenkron Compute:** RX 9070'in RDNA 4 mimarisi, grafik işlerini yaparken aynı anda bağımsız hesaplama (Compute) kuyruklarını yürütebilir. C++ tarafında `Async Compute` thread'leri oluşturun.
3. **Resizable BAR Entegrasyonu:** HAGS ve Re-Size BAR açık olduğu için, CPU'nun GPU belleğine büyük bloklar halinde erişimini sağlayacak `Memory Mapping` tekniklerini kullanın.

---

## 4. Uygulama Yaşam Döngüsü ve Başlatma (Initialization)

Antigravity C++ uygulamanız başlarken şu kontrol listesini takip etmelidir:

* [ ] **Adapter Discovery:** Sistemdeki Vendor ID (0x1002) üzerinden RX 9070 ve 890M ayrımını yap.
* [ ] **Bus Speed Check:** PCIe link hızının Gen4 x4 olduğunu doğrula.
* [ ] **NPU Handshake:** Ryzen AI motorunun hazır olup olmadığını ve sürücü versiyonunu kontrol et.
* [ ] **Shader Pre-compilation:** RDNA 4 mimarisi için shader'ları uygulama ilk açılışında (veya kurulumda) önbelleğe al.

---

## 5. Geleceğe Yönelik Notlar (RDNA 4 & FSR 4)

RX 9070'in en büyük gücü **Yapay Zeka tabanlı FSR 4** desteğidir. Geliştireceğiniz C++ uygulamasında Antigravity'nin post-processing aşamasına AI tabanlı upscaling katmanlarını eklemek, 4K çözünürlükte akıcı performans almanızı sağlayacaktır.

---
**Hazırlayan:** Gemini (Sizin İçin Optimize Edilmiştir)  
**Tarih:** Mart 2026