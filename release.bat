@echo off
setlocal
echo ===================================================
echo HybridCore - Production Release Builder
echo ===================================================

:: Microsoft Visual Studio yolunu vswhere ile bulup ortami baslatalim
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
    echo [UYARI] Visual Studio vswhere ile bulunamadi. PATH uzerinde cl.exe oldugu varsayiliyor.
)

where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] MSVC Derleyicisi bulunamadi. Lutfen Developer Command Prompt uzerinden calistirin.
    goto :eof
)

echo [1/5] Derleme Dizinleri Kuruluyor...
if not exist "bin" mkdir bin
if not exist "bin\shaders" mkdir bin\shaders
:: Onceki surum varsa temizle
if exist "HybridCore_Release" rmdir /S /Q HybridCore_Release
mkdir HybridCore_Release
mkdir HybridCore_Release\bin
mkdir HybridCore_Release\bin\shaders
mkdir HybridCore_Release\bin\models

echo [2/5] HLSL Compute Shader Derleniyor (Donanim Optimizasyonlu)...
fxc.exe /nologo /T cs_5_0 /E main /O3 /Fo bin\shaders\compute.cso src\shaders\compute.hlsl
if %errorlevel% neq 0 (
    echo [ERROR] Shader derleme basarisiz!
    goto :eof
)

echo [3/5] C++ Motoru MAXIMUM Release (/O2 /MT /D NDEBUG) Modunda Derleniyor...
:: /O2: Maximum Speed, /MT: Statik Multi-Thread Runtime (Baska PC'de VCRUNTIME dll istememesi icin)
cl /EHsc /O2 /MT /std:c++20 /D NDEBUG ^
    src\main.cpp src\core\Window.cpp src\core\JobRouter.cpp ^
    src\core\Engine.cpp src\graphics\ComputeBenchmarker.cpp src\ai\NpuEngine.cpp ^
    /Fe:bin\HybridCoreDiscovery.exe ^
    /Fo:bin\ ^
    user32.lib dxgi.lib d3d11.lib d3dcompiler.lib windowsapp.lib > build_release.log

if exist bin\*.obj del bin\*.obj

if %errorlevel% neq 0 (
    echo [ERROR] C++ Derleme basarisiz!
    type build_release.log
    goto :eof
)

echo [4/5] Release Paketi Modelleniyor (Assetler Kopyalaniyor)...
copy /Y bin\HybridCoreDiscovery.exe HybridCore_Release\bin\ >nul
copy /Y bin\shaders\compute.cso HybridCore_Release\bin\shaders\ >nul

if exist bin\models\mnist-8.onnx (
    copy /Y bin\models\mnist-8.onnx HybridCore_Release\bin\models\ >nul
) else (
    echo [UYARI] bin\models\mnist-8.onnx modeli bulunamadi, lutfen elle 'HybridCore_Release\bin\models\' icerisine ekleyin.
)

:: Baslatici Bat Dosyasi (dizini kok referans almak icin onemli)
echo @echo off > HybridCore_Release\Start_HybridCore.bat
echo setlocal >> HybridCore_Release\Start_HybridCore.bat
echo cd /d "%%~dp0" >> HybridCore_Release\Start_HybridCore.bat
echo echo [HybridCore] Asimetrik motor baslatiliyor... >> HybridCore_Release\Start_HybridCore.bat
echo bin\HybridCoreDiscovery.exe >> HybridCore_Release\Start_HybridCore.bat
echo endlocal >> HybridCore_Release\Start_HybridCore.bat

echo [5/5] Uygulama Portable Hale Getiriliyor (Zip)...
powershell -Command "Compress-Archive -Path HybridCore_Release -DestinationPath HybridCore_Release.zip -Force"

echo ===================================================
echo [BASARILI] "HybridCore_Release.zip" arsivi olusturuldu!
echo ===================================================
echo Nasil Kullanilir?
echo 1) HybridCore_Release.zip dosyasini hedef Windows 11/10 bilgisayarina gonderin.
echo 2) Zip uzerine sag tiklayip klasore cikartin.
echo 3) Cikan klasorun icindeki "Start_HybridCore.bat" dosyasina cift tiklayip sistemi atesleyin.
echo ===================================================
endlocal
