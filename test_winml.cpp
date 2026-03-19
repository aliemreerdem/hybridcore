#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.AI.MachineLearning.h>
#include <iostream>

using namespace winrt;
using namespace winrt::Windows::AI::MachineLearning;

int main() {
    init_apartment();
    try {
        auto device = LearningModelDevice(LearningModelDeviceKind::DirectXHighPerformance);
        std::cout << "WinML Device created successfully on High Performance Compute Unit!" << std::endl;
    } catch (...) {
        std::cout << "WinML Device creation failed." << std::endl;
    }
    return 0;
}
