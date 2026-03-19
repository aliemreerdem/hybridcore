#include "core/Engine.h"
#include <iostream>
#include <vector>
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <fstream>
#include <string>
#include <stdexcept>
#include <winrt/base.h>

#pragma comment(lib, "dxgi.lib")
using Microsoft::WRL::ComPtr;

// --- Global Error Logger ---
void LogFatalError(const std::string& message) {
    std::ofstream logFile("error.log", std::ios::app);
    if (logFile.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char timeBuf[128];
        sprintf_s(timeBuf, "[%04d-%02d-%02d %02d:%02d:%02d] FATAL ERROR: ", 
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        logFile << timeBuf << message << "\n";
    }
    std::cerr << "\n[FATAL ERROR] " << message << std::endl;
}

// --- Windows SEH (Structured Exception Handling) Filter ---
LONG WINAPI GlobalUnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo) {
    DWORD exceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
    char hexBuffer[64];
    sprintf_s(hexBuffer, "Unhandled SEH Exception! Code: 0x%08lX", exceptionCode);
    
    std::string details = std::string(hexBuffer);
    if (exceptionCode == EXCEPTION_ACCESS_VIOLATION) details += " (ACCESS_VIOLATION)";
    else if (exceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO) details += " (INT_DIVIDE_BY_ZERO)";
    else if (exceptionCode == EXCEPTION_STACK_OVERFLOW) details += " (STACK_OVERFLOW)";

    LogFatalError(details);
    return EXCEPTION_EXECUTE_HANDLER; // Allows the OS to terminate securely
}

// Donanim sorgulama kodumuzu test/bilgilendirme amaciyla tutuyoruz
void EnumerateAdapters() {
    ComPtr<IDXGIFactory6> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return;

    std::cout << "[DXGI] System Components (Summary):\n";
    ComPtr<IDXGIAdapter1> adapter;
    UINT adapterIndex = 0;
    while (factory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
             std::wcout << L"  -> " << desc.Description << L" (VRAM: " << (desc.DedicatedVideoMemory / (1024 * 1024)) << L" MB)\n";
        }
        adapterIndex++;
        adapter.Reset();
    }
    std::cout << "------------------------------------------\n\n";
}

int main() {
    // 1. Install Global SEH Filter
    SetUnhandledExceptionFilter(GlobalUnhandledExceptionFilter);

    // Konsol karakter destek formati
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "--- HybridCore AI Analysis Tool ---\n" << std::endl;
    
    try {
        // Once donanim listesini bas
        EnumerateAdapters(); 

        // Motoru baslat ve pencereli donguye gir
        core::Engine engine;
        engine.Initialize();
        engine.Run();
        engine.Shutdown();
    }
    catch (const winrt::hresult_error& ex) {
        std::wstring msg = ex.message().c_str();
        std::string narrowMsg(msg.begin(), msg.end());
        LogFatalError("WinRT HRESULT Exception: " + narrowMsg);
        return -1;
    }
    catch (const std::exception& ex) {
        LogFatalError("Standard C++ Exception: " + std::string(ex.what()));
        return -1;
    }
    catch (...) {
        LogFatalError("Unknown/Generic C++ Exception caught in main().");
        return -1;
    }

    return 0;
}
