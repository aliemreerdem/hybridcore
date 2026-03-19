// Real eGPU Compute Workload Simulator
// Bu shader RX 9070 ustunde calisacak ve donanimin asil islem birimlerini (CU) zorlayacaktir.

RWStructuredBuffer<float> outputBuffer : register(u0);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    float value = (float)DTid.x;
    
    // Ağır Matematik Döngüsü: Sensör verisini simüle eder
    // GPU paralellikte şampiyon olduğu için bu döngüyü binlerce iş parçacığında anında yürütecektir.
    for (int i = 0; i < 50000; ++i) {
        value = sin(value) * cos(value) + tan(value * 0.5f);
    }
    
    // Sonucu buffer'a yaz (derleyicinin döngüyü silmesini önlemek için)
    outputBuffer[DTid.x] = value;
}
