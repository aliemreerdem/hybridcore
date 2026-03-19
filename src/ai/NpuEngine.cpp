#include "NpuEngine.h"
#include <iostream>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::AI::MachineLearning;

namespace ai {

NpuEngine::NpuEngine() {
    // WinRT COM Apartment basyapi hazirligi
    winrt::init_apartment();
    
    // NPU'yu kitlemek icin 28x28 boyutunda Dummy Float m_dummyInput hazirla
    m_dummyInput.resize(1 * 1 * 28 * 28, 0.5f);
}

NpuEngine::~NpuEngine() {
    // Note: C++/WinRT handles uninit_apartment automatically via caching wrappers,
    // but we can leave it to RAII.
}

bool NpuEngine::Initialize(const std::wstring& modelPath) {
    try {
        // ONNX modelini diskten WinML (DirectML) engine'ine al
        m_model = LearningModel::LoadFromFilePath(modelPath);

        // Default birakarak Windows 11'in otomatik NPU/MCDM routing mekanizmasina (MCDM API) isin donmesini umuyoruz
        auto device = LearningModelDevice(LearningModelDeviceKind::Default);
        
        m_session = LearningModelSession(m_model, device);
        m_binding = LearningModelBinding(m_session);

        // MNIST (mnist-8.onnx) ONNX modelinin Input Tensor adi "Input3" (1x1x28x28 Float)
        auto shape = winrt::single_threaded_vector<int64_t>({ 1, 1, 28, 28 });
        auto view = winrt::single_threaded_vector<float>(std::vector<float>(m_dummyInput));
        auto tensor = TensorFloat::CreateFromIterable(shape, view);
        m_binding.Bind(L"Input3", tensor); 

        std::cout << "[NpuEngine] WinML Session Ready! ONNX model bound to NPU/DirectX inference APIs." << std::endl;
        return true;
    } catch (winrt::hresult_error const& ex) {
        std::wcerr << L"[NpuEngine] WinML Initialization Failed (HRESULT): " << ex.message().c_str() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[NpuEngine] WinML Initialization Failed: Unknown exception." << std::endl;
        return false;
    }
}

void NpuEngine::EvaluateDummyPayload() {
    if (!m_session) return;
    
    try {
        // Bu thread (NPU_Worker) uzerinden ilk cagrida COM apartment yarat.
        thread_local bool t_initialized = false;
        if (!t_initialized) {
            winrt::init_apartment();
            t_initialized = true;
        }

        // NPU/DirectML uzerinden ONNX Inference Request tetikle
        // Agir yapay zeka Tensor islemleri baslasin
        // Kullanimin %0'da kalmamasi icin ayni istegi 500 kez ust uste isletip donanimi gercekten zorlayacagiz
        for (int i = 0; i < 500; ++i) {
            auto results = m_session.Evaluate(m_binding, L"DummyEvaluation");
        }
    } catch (...) {
        // Continuous simülasyonda sistem çokması yasanmasin diye yut
    }
}

} // namespace ai
