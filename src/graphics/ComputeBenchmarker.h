#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <mutex>

using Microsoft::WRL::ComPtr;

namespace graphics {

class ComputeBenchmarker {
public:
    ComputeBenchmarker();
    ~ComputeBenchmarker();

    bool Initialize(UINT adapterIndex);
    void DispatchWorkload();
    void CheckForShaderUpdates();

private:
    bool LoadShader(const std::wstring& path);
    bool CompileAndSwapShader(const std::wstring& hlslPath);

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<ID3D11ComputeShader> m_computeShader;
    std::mutex m_shaderMutex;
    
    // Shader'in yazacagi hedef bellek
    ComPtr<ID3D11Buffer> m_uavBuffer;
    ComPtr<ID3D11UnorderedAccessView> m_uav;

    FILETIME m_lastShaderWriteTime;
    std::wstring m_shaderSourcePath;
};

} // namespace graphics
