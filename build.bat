@echo off
setlocal

REM Microsoft Visual Studio yolunu vswhere ile bulalim
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

REM MSVC Build ortamini baslat (64-bit)
if defined VS_PATH (
    if exist "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" (
        call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    )
) else (
    echo Uyari: Visual Studio vswhere ile bulunamadi. PATH uzerinde cl.exe oldugu varsayiliyor.
)

REM Cikti klasorunu olustur
if not exist bin\shaders mkdir bin\shaders
echo Compiling HLSL Compute Shader (fxc.exe)...
fxc.exe /nologo /T cs_5_0 /E main /Fo bin\shaders\compute.cso src\shaders\compute.hlsl
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Shader derleme basarisiz!
    exit /b %ERRORLEVEL%
)

echo Compiling HybridCore Subsystems...
cl.exe /nologo /EHsc /W4 /std:c++20 /await /MT src\main.cpp src\core\Window.cpp src\core\JobRouter.cpp src\core\Engine.cpp src\graphics\ComputeBenchmarker.cpp src\ai\NpuEngine.cpp /Fobin\ /Febin\HybridCoreDiscovery.exe /link user32.lib dxgi.lib d3d11.lib windowsapp.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Derleme Build basarisiz oldu!
    exit /b %ERRORLEVEL%
)

echo.
echo Derleme Basarili. Uygulamayi baslatmak icin lutfen asagidaki komutu girin:
echo bin\HybridCoreDiscovery.exe
echo ------------------------------------------------
