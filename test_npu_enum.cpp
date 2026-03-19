#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.AI.MachineLearning.h>
#include <iostream>

using namespace winrt;
using namespace winrt::Windows::AI::MachineLearning;

int main() {
    init_apartment();
    try {
        // Test if the compiler knows 'Npu'
        auto kind = LearningModelDeviceKind::Npu;
        std::cout << "LearningModelDeviceKind::Npu exists! Enum value: " << (int)kind << std::endl;
    } catch (...) {
        std::cout << "Error" << std::endl;
    }
    return 0;
}
