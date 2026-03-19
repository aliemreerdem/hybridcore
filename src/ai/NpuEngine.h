#pragma once
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.AI.MachineLearning.h>
#include <string>
#include <vector>

namespace ai {

class NpuEngine {
public:
    NpuEngine();
    ~NpuEngine();

    bool Initialize(const std::wstring& modelPath);
    void EvaluateDummyPayload();

private:
    winrt::Windows::AI::MachineLearning::LearningModel m_model = nullptr;
    winrt::Windows::AI::MachineLearning::LearningModelSession m_session = nullptr;
    winrt::Windows::AI::MachineLearning::LearningModelBinding m_binding = nullptr;
    
    // NPU (Ryzen AI) icin agir tensor hesaplama payload'u (Dummy MNIST Matrix)
    std::vector<float> m_dummyInput;
};

} // namespace ai
