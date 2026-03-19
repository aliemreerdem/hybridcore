@echo off
setlocal
echo ===================================================
echo HybridCore - Production Release Builder
echo ===================================================

:: Locate Microsoft Visual Studio path using vswhere and initialize environment
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

if defined VS_PATH (
    if exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
        call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    )
) else (
    echo [WARNING] Visual Studio not found via vswhere. Assuming cl.exe is in PATH.
)

where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] MSVC Compiler not found. Please run from Developer Command Prompt.
    goto :eof
)

echo [1/5] Setting up Build Directories...
if not exist "bin" mkdir bin
if not exist "bin\shaders" mkdir bin\shaders
:: Clean up previous version if exists
if exist "HybridCore_Release" rmdir /S /Q HybridCore_Release
mkdir HybridCore_Release
mkdir HybridCore_Release\bin
mkdir HybridCore_Release\bin\shaders
mkdir HybridCore_Release\bin\models

echo [2/5] Compiling HLSL Compute Shader (Hardware Optimized)...
fxc.exe /nologo /T cs_5_0 /E main /O3 /Fo bin\shaders\compute.cso src\shaders\compute.hlsl
if %errorlevel% neq 0 (
    echo [ERROR] Shader compilation failed!
    goto :eof
)

echo [3/5] Compiling C++ Engine in MAXIMUM Release (/O2 /MT /D NDEBUG) Mode...
:: /O2: Maximum Speed, /MT: Static Multi-Thread Runtime (prevents VCRUNTIME dll dependency on other PCs)
cl /EHsc /O2 /MT /std:c++20 /D NDEBUG ^
    src\main.cpp src\core\Window.cpp src\core\JobRouter.cpp ^
    src\core\Engine.cpp src\graphics\ComputeBenchmarker.cpp src\ai\NpuEngine.cpp ^
    /Fe:bin\HybridCoreDiscovery.exe ^
    /Fo:bin\ ^
    user32.lib dxgi.lib d3d11.lib d3dcompiler.lib windowsapp.lib > build_release.log

if exist bin\*.obj del bin\*.obj

if %errorlevel% neq 0 (
    echo [ERROR] C++ Compilation failed!
    type build_release.log
    goto :eof
)

echo [4/5] Packaging Release Build (Copying Assets)...
copy /Y bin\HybridCoreDiscovery.exe HybridCore_Release\bin\ >nul
copy /Y bin\shaders\compute.cso HybridCore_Release\bin\shaders\ >nul

if exist bin\models\mnist-8.onnx (
    copy /Y bin\models\mnist-8.onnx HybridCore_Release\bin\models\ >nul
) else (
    echo [WARNING] bin\models\mnist-8.onnx model not found, please add manually to 'HybridCore_Release\bin\models\'.
)

:: Launcher Batch File (ensures absolute root directory reference)
echo @echo off > HybridCore_Release\Start_HybridCore.bat
echo setlocal >> HybridCore_Release\Start_HybridCore.bat
echo cd /d "%%~dp0" >> HybridCore_Release\Start_HybridCore.bat
echo echo [HybridCore] Initializing asymmetric hardware engine... >> HybridCore_Release\Start_HybridCore.bat
echo bin\HybridCoreDiscovery.exe >> HybridCore_Release\Start_HybridCore.bat
echo endlocal >> HybridCore_Release\Start_HybridCore.bat

echo [5/5] Creating Portable Application Archive (Zip)...
powershell -Command "Compress-Archive -Path HybridCore_Release -DestinationPath HybridCore_Release.zip -Force"

echo ===================================================
echo [SUCCESS] "HybridCore_Release.zip" archive has been created!
echo ===================================================
echo How to use?
echo 1) Transfer the HybridCore_Release.zip file to the target Windows 11/10 PC.
echo 2) Right-click the Zip and extract to a folder.
echo 3) Double-click "Start_HybridCore.bat" inside the extracted folder to ignite the system.
echo ===================================================
endlocal
