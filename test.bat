@echo off
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
  set InstallDir=%%i
)
call "%InstallDir%\VC\Auxiliary\Build\vcvars64.bat"
cl.exe /nologo /EHsc /std:c++20 /await test_npu_enum.cpp /link windowsapp.lib
.\test_npu_enum.exe
